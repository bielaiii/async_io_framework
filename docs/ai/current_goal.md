# Current Goal

Build a modern C++ coroutine-based async IO framework for Linux/WSL.

The framework should provide awaitable operations so developers can write
network IO code in a direct coroutine style while the runtime handles epoll
registration, readiness notification, timer wakeups, cancellation, and fd
lifetime details.

## Core Capabilities

- Provide awaitable APIs for asynchronous network IO.
- Support async connect, accept, read, read-some, and write operations.
- Support timer awaitables such as `wait_for`.
- Support operation timeouts for IO paths that need bounded waiting.
- Support custom cancellation tokens so callers can cancel pending async work.
- Wake suspended operations actively when cancellation is requested, instead of
  relying on future socket readiness.
- Preserve explicit ownership and lifetime boundaries around fds, coroutine
  handles, operation state, and scheduler registrations.

## Technical Direction

- Use modern C++ features, currently C++20-oriented coroutine design with
  newer compiler support where useful.
- Keep core async paths lightweight and high-performance.
- Prefer RAII for resource ownership and non-owning viewer types for borrowed
  fd access.
- Avoid exceptions in hot async completion paths where practical.
- Use Linux primitives directly, especially epoll, timerfd, eventfd, and
  non-blocking sockets.
- Use per-operation completion arbitration so socket readiness, timeout, and
  cancellation races resolve deterministically.
- Keep awaitable behavior testable through CTest/Catch2 unit tests rather than
  only manual demo programs.

## Current Quality Bar

- The framework should be usable as a small async networking foundation, not
  just a set of examples.
- Tests should verify observable behavior: successful IO, timeout, timer
  wakeup, cancellation, and buffer state.
- Manual test programs may remain for exploration, but new behavior should
  have automated tests when it is stable enough to assert.
- Changes should stay small and buildable, with special care around fd
  lifetime, epoll registration lifetime, and coroutine frame lifetime.

## Future Improvements

- Replace the current timer dispatch model with a stable timer dispatcher
  operation registered on the shared `timerfd`.
- Let `TimerEvent` dispatch all expired timers in one pass, including timers
  with equal deadlines, instead of exposing only the current user timer
  operation to the scheduler.
- Keep timer cancellation cheap through timer ids and tombstones, while making
  expired-task dispatch more mature.
- Expand automated coverage for active cancellation races, especially cases
  where socket readiness, timeout, and cancellation become ready in the same
  scheduler batch.
- Clarify and enforce the current single-waiter socket model: one pending read
  and one pending write per socket fd.
- Consider a future scheduler model that can either reject or safely queue
  multiple simultaneous operations on the same fd.
- Improve public API polish around cancellation tokens, timeout composition,
  connection ownership, and non-owning fd viewers.
- Grow integration tests that start local loopback servers inside the test
  process instead of relying on manually started services.
- Continue reducing exception use in hot async paths while keeping setup and
  resource acquisition failures diagnosable.
