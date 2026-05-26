# Decision Log

This file records project-level design decisions that should guide future
implementation work. Detailed rationale lives in the linked design notes.

## Active Cancellation for Async Operations

Decision: pending async operations should use active cancellation.

When cancellation is requested, the operation should not wait for the socket,
timer, accept, or connect fd to become ready again. A real `cancel_token`
provides an `eventfd` that wakes `epoll_wait`; the scheduler dispatches that
cancel event; the operation resumes, finishes as cancelled if cancellation wins,
and removes its registered interests.

Rationale:

- Lazy cancellation can leave a coroutine suspended forever when no future fd
  readiness event arrives.
- Cancellation must be observable by the scheduler, not only by a flag inside
  the coroutine frame.
- Socket readiness, timer expiry, and cancellation can race, so each operation
  needs explicit completion arbitration.

Implementation direction:

- Keep `cancel_token` responsible for shared cancellation state and the cancel
  wake fd.
- Keep `operation_state` responsible for deciding whether completion,
  timeout, or cancellation wins for one pending operation.
- Keep `cancel_slot` as the adapter that registers a cancel fd interest with
  the scheduler when the token is event-backed.
- Keep `noop_cancel_token` / `no_cancel_token` as the zero-overhead path when
  cancellation is not needed.

Reference: `docs/active_cancellation_design_note.md`

## Cancellation Broadcast Is Separate From Socket Interest

Decision: scheduler cancel fd registration is allowed to have multiple waiting
operations, but socket fds remain single-waiter for read and write.

The same `cancel_token` may be attached to multiple pending operations, so the
scheduler stores cancel waiters as a list and completes all registered cancel
read operations when the cancel fd becomes readable.

Socket readiness remains simpler:

- one read operation per socket fd
- one write operation per socket fd

Implication: users should not start multiple simultaneous reads on the same fd
or multiple simultaneous writes on the same fd until the scheduler model is
expanded.

Reference: `docs/active_cancellation_design_note.md`

## Timer Cancellation Uses Timer IDs And Tombstones

Decision: `TimerEvent` uses lazy cancellation for queued timer tasks.

Each scheduled timer task receives a unique id. The active map records:

```text
read_op* -> timer id
```

Cancelling a timer records the id in `cancelled_timer_ids`; cancelled heap
entries are skipped when `TimerEvent` advances. This avoids rebuilding the
priority queue on every non-current timer cancellation.

Rationale:

- Removing an arbitrary entry from a priority queue is expensive.
- Multiple timers may share the same deadline, so `time_point` is not a safe
  identity.
- `read_op*` alone is not stable enough as a tombstone because coroutine frame
  or awaiter storage may later reuse the same address.

Reference: `docs/timer_event_design_note.md`

## Timer Dispatch Should Move Toward A Stable Dispatcher

Decision: the current timer dispatch model is acceptable for the present
implementation, but the preferred future design is a stable timer dispatcher
registered with the scheduler.

Current limitation:

- Only the current timer task is registered with the scheduler.
- Equal-deadline or already-expired timers are processed one by one.
- `TimerEvent` exposes the current user timer operation directly to the
  scheduler.

Future direction:

- Register one stable dispatcher operation for the shared `timerfd`.
- Let `TimerEvent` own expired-task dispatch.
- On timerfd readiness, consume the timerfd, pop cancelled tasks, resume every
  expired timer, then arm the next future timer or unregister the timerfd.

This should better separate responsibilities:

```text
Scheduler  -> watches fd readiness
TimerEvent -> owns timer ordering, cancellation, and expired timer dispatch
```

Reference: `docs/timer_event_design_note.md`

## Testing Direction

Decision: stable behavior should be covered by Catch2/CTest tests rather than
manual demo programs only.

Manual programs are still useful while exploring network behavior, but core
semantics should become automated tests when they can be asserted reliably.
Important coverage areas include:

- buffer state transitions
- successful async read/write/connect
- timer wakeup
- timeout
- active cancellation
- fd and coroutine lifetime edge cases

This keeps the framework moving toward a reusable library instead of a set of
experiments.
