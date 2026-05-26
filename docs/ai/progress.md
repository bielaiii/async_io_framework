# Progress

## Current State

The project is moving from manual async IO demos toward a testable coroutine
networking framework.

The core direction is now documented:

- `docs/ai/current_goal.md` describes the framework goal and future work.
- `docs/ai/decision_log.md` records the main design decisions.
- `docs/active_cancellation_design_note.md` explains active cancellation.
- `docs/timer_event_design_note.md` explains current timer cancellation and the
  preferred future timer dispatcher design.

## Completed

- CTest is enabled from the top-level CMake project.
- Catch2 is integrated for automated unit tests.
- CMake first tries `find_package(Catch2 3 QUIET)` and falls back to
  `FetchContent` for Catch2 v3.7.1.
- Existing manual test executables remain buildable but are not registered as
  CTest tests.
- `BufferTest` was added for buffer behavior.
- `AsyncIoTest` was added for coroutine IO behavior using in-process fds.
- `StaticBuffer::capacity()` was fixed to return `std::array::size()` instead
  of calling a non-existent `std::array::capacity()`.

## Automated Coverage

CTest currently discovers 9 tests:

- `StaticBuffer tracks remaining capacity from its offset`
- `DynamicBuffer tracks written bytes and can be reused`
- `Buffer_View is a non-owning view over caller storage`
- `async_read completes with bytes already available on the fd`
- `async_write sends the full buffer to a ready fd`
- `async_connect connects to a local TCP listener`
- `async_read_timeout reports timed_out when no bytes arrive`
- `async_read completes immediately when cancellation is pre-requested`
- `wait_for resumes after the timer expires`

The async IO tests avoid external services:

- `socketpair()` is used for local read/write tests.
- A temporary loopback TCP listener bound to port `0` is used for connect tests.
- Timer and timeout tests use short durations and run through the real
  scheduler.

## Design Status

Active cancellation is the intended cancellation model.

- `cancel_token` owns shared cancellation state and an `eventfd`.
- `operation_state` arbitrates whether completion, timeout, or cancellation
  wins for a specific operation.
- `cancel_slot` adapts event-backed tokens into scheduler registrations.
- `noop_cancel_token` / `no_cancel_token` remain the no-cancellation path.

Timer cancellation currently uses lazy cancellation.

- Each timer gets a unique id.
- Cancelled timers are recorded as tombstones.
- Cancelled heap entries are skipped when the timer queue advances.

## Known Limitations

- The scheduler still supports only one pending read operation and one pending
  write operation per socket fd.
- Cancel fd registration can broadcast to multiple waiters, but socket fd
  readiness slots are still single-waiter slots.
- `TimerEvent` still registers the current user timer operation directly with
  the scheduler.
- Equal-deadline timers are not dispatched in one timerfd event yet.
- Manual test programs still contain external port assumptions or infinite
  loops, so they are intentionally not part of CTest.
- More race coverage is needed for cases where socket readiness, timer expiry,
  and cancellation are all ready in the same scheduler batch.

## Next Work

- Implement a stable timer dispatcher operation for the shared `timerfd`.
- Add tests for multiple timers with equal or already-expired deadlines.
- Add tests for active cancellation after an operation is already suspended.
- Add race tests for completion vs timeout vs cancellation.
- Decide how the scheduler should handle multiple same-fd reads or writes:
  reject, assert, queue, or support multiple waiters explicitly.
- Convert useful manual tests into self-contained Catch2 tests with local
  loopback resources.
- Improve public API clarity around cancellation, timeout composition,
  connection ownership, and fd viewer lifetimes.
