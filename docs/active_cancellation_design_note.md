# Active Cancellation Design Note

## Decision

The framework uses active cancellation for asynchronous I/O operations.

When a user requests cancellation, a pending operation should not wait for the
socket to become readable or writable again. The cancellation request should wake
the scheduler, let the operation finish as cancelled, and remove the operation's
registered interests from epoll.

This is different from lazy cancellation. Lazy cancellation only records a flag
and waits until the operation naturally runs again. That is not a good default
for this framework because a pending socket read, write, accept, connect, or
timer wait may have no future readiness event.

## Pending I/O

In this framework, pending I/O means:

```text
an async operation has registered fd readiness with Scheduler and suspended its coroutine
```

For example, an `async_read` that cannot read immediately registers:

```text
socket fd -> read operation
```

and then suspends. The coroutine will not run again until the scheduler resumes
it. If cancellation only updates an atomic flag, the coroutine may never observe
that flag because `epoll_wait` may still be asleep.

## Cancellation Token Role

`cancel_token` has two responsibilities:

1. Store the shared cancellation request state.
2. Provide an `eventfd` that can wake `epoll_wait`.

The token does not directly remove socket or timer interests. It only makes a
cancel fd readable. The scheduler then dispatches the cancel fd to the operation
that registered interest in it.

The flow is:

```text
request_cancel()
    -> cancelled = true
    -> write(cancel eventfd, uint64_t{1})
    -> epoll_wait returns cancel fd as readable
    -> Scheduler dispatches cancel read_op
    -> operation_state tries to finish as CANCELLED
    -> coroutine resumes and cleans its registered interests
```

## Operation State Role

`cancel_token` only answers:

```text
was cancellation requested?
```

It does not answer:

```text
has this specific operation already completed?
did socket readiness win?
did timeout win?
did cancellation win?
```

Each pending operation therefore owns an `operation_state`. Socket readiness,
timer expiry, and cancellation all try to finish the same state:

```text
socket ready  -> COMPLETED
timer expired -> TIMEOUT
cancel ready  -> CANCELLED
```

Only the first event source that changes the state from `INPROGRESS` may resume
the coroutine. Later events observe that the operation is already finished and do
nothing.

This is needed because epoll may return multiple ready events in the same batch,
for example both the socket fd and timer fd.

## Scheduler Cancel Broadcast

The scheduler stores normal socket interests as one read slot and one write
slot:

```text
socket fd -> read_op*
socket fd -> write_op*
```

Cancel fds are different. The same `cancel_token` may be attached to more than
one pending operation, and all of them should observe the cancellation request.

The scheduler therefore keeps a separate cancel-read list:

```text
cancel fd -> vector<read_op*>
```

When the cancel fd becomes readable, the scheduler completes every registered
cancel read operation. Each operation's own `operation_state` decides whether
that cancellation actually wins.

## Cleanup Rules

When an operation resumes, it cleans up according to the winning result.

For socket read:

```text
COMPLETED -> remove cancel interest, remove socket read interest, read socket
CANCELLED -> consume cancel fd, remove socket read interest, remove cancel interest
```

For timeout read/write:

```text
COMPLETED -> cancel timer task, remove cancel interest, remove socket interest, perform I/O
TIMEOUT   -> consume timerfd, cancel timer task, remove socket interest
CANCELLED -> consume cancel fd, cancel timer task, remove socket interest, remove cancel interest
```

For `wait_for`:

```text
COMPLETED -> consume timerfd and advance TimerEvent
CANCELLED -> cancel timer task and remove cancel interest
```

Cancellation does not drain or discard TCP data. If a socket operation is
cancelled or times out, the socket remains open and unread data stays in the
kernel receive buffer for a future operation.

## Current Coverage

The current implementation uses active cancellation for operations that can now
accept a cancel token:

```text
async_read
async_read_timeout
async_read_some
async_write
async_write_timeout
async_accept
async_connect
wait_for
```

Each of these uses `operation_state` and `cancel_slot` when a real
event-backed `cancel_token` is provided. With `noop_cancel_token` /
`no_cancel_token`, no cancel fd is registered.

Legacy executor types that do not expose a cancel token are outside the current
cancellation surface.

## Known Limitations

The scheduler still has a simple readiness model for sockets:

```text
one read operation per socket fd
one write operation per socket fd
```

That means the framework should not run multiple simultaneous read operations on
the same socket fd. The cancel fd can broadcast to multiple operations, but the
socket fd read/write slots are still single-waiter slots.

Timer dispatch also remains simple. `TimerEvent` still exposes the current timer
task to the scheduler instead of using a stable timer dispatcher operation. The
future direction is documented in `timer_event_design_note.md`.

## Naming Guidance

Use distinct names for distinct cancellation layers:

```text
cancel_token        -> external cancellation request
operation_state     -> per-operation completion arbitration
remove_read/write   -> scheduler fd interest removal
cancel timer task   -> remove or lazily skip a timer queue entry
```

These are related, but they should not be treated as the same operation.
