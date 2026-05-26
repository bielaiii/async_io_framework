# TimerEvent Design Note

## Background

`TimerEvent` uses one Linux `timerfd` to drive multiple coroutine timers. Each user timer is stored as a `TimerTask` and ordered by `time_point` in a `std::priority_queue`.

The original cancellation implementation removed a timer by scanning the whole priority queue, copying every non-cancelled task into a temporary `remaining` vector, and rebuilding the queue. That works, but each non-current cancellation costs `O(n)`.

## Lazy Cancellation

The current design uses lazy cancellation:

1. Each `TimerTask` gets a unique `id`.
2. `active_timer_ids` maps the concrete `read_op*` used by an awaiter to its timer id.
3. `cancel(read_op*)` records the timer id in `cancelled_timer_ids` instead of removing the task from the heap immediately.
4. When `TimerEvent` advances to the next timer, cancelled heap entries are popped and skipped.

This keeps cancellation cheap and avoids rebuilding the priority queue.

The timer id is important. Multiple timers can have the same delay, for example five timers all scheduled for 4 seconds. They cannot be identified by `time_point`; they are identified by the operation that should be resumed. The relation is:

```text
read_op* -> timer id -> TimerTask
```

Using only `read_op*` as the tombstone would be risky because coroutine frames or awaiters may later reuse the same address. The timer id makes each scheduled timer instance distinct.

## Remaining Design Issue

`TimerEvent` still has a deeper dispatch limitation:

```cpp
TimerTask cur;
std::priority_queue<TimerTask, std::vector<TimerTask>, TimerTaskComp> waiting_queue;
```

Only `cur` is registered with the scheduler. When the shared `timerfd` becomes readable, the scheduler resumes the operation stored in `cur.state_.read`.

This means timers with the same or already-expired deadline are processed one by one:

1. `timerfd` fires for `cur`.
2. The current awaiter resumes.
3. `TimerEvent` advances to the next valid task.
4. `timerfd_settime(..., TFD_TIMER_ABSTIME, ...)` arms the timerfd for that task.
5. If that deadline has already passed, the next event is triggered shortly after.

That behavior can work, but it is not ideal. A mature event-loop design should let the timerfd event dispatch all expired timers in one pass.

## Preferred Future Direction

Instead of registering the current user timer operation directly with the scheduler, register one stable timer dispatcher operation for the timerfd.

Conceptually:

```cpp
struct timer_dispatch_op {
    TimerEvent *timer;
    Scheduler *scheduler;

    void complete() noexcept {
        timer->consume();
        timer->complete_expired(*scheduler);
    }
};
```

Then `TimerEvent::complete_expired` can:

1. Read and consume the timerfd expiration count.
2. Get `now`.
3. Pop cancelled tasks.
4. Resume every task whose `time_point <= now`.
5. Arm the timerfd for the next non-cancelled future task.
6. Remove the timerfd from the scheduler if no timers remain.

That design separates two responsibilities:

```text
scheduler: watches timerfd readiness
TimerEvent: owns timer ordering, cancellation, and expired-task dispatch
```

It also handles equal-deadline timers naturally because every expired `TimerTask` can be resumed during the same timerfd event.

