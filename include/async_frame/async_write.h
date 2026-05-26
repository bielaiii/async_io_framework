#ifndef ASYNC_WRITE_HEADER
#define ASYNC_WRITE_HEADER

#include "base.h"
#include "buffer.h"
#include "cancellation_token.h"
#include "connecting.h"
#include "connection.h"
#include "do_write.h"
#include "operator.h"
#include "scheduler.h"
#include "scheduler_type_traits.h"
#include "single_operator.h"
#include "task.h"
#include <cerrno>
#include <chrono>
#include <coroutine>
#include <cstddef>
#include <fstream>
#include <memory>
#include <span>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
namespace ASYNC_FRAME {
namespace detail {

/* 据说是约定俗称的接口 */
/* void handler(std::error_code ec, std::size_t bytes_transferred); */

template <typename SCHEDULER, typename CONNECTION_LIKE, typename BUFFER_VIEW,
          typename CANCEL_TOKEN = noop_cancel_token>
struct inner_send_awaiter {
  Base<CONNECTION_LIKE, BUFFER_VIEW, CANCEL_TOKEN> base_;
  SCHEDULER &scheduler;
  fd_ops state_;
  write_op write_;
  [[no_unique_address]] cancel_slot<CANCEL_TOKEN> cancel_;

  bool await_ready() noexcept { return false; }

  bool await_suspend(std::coroutine_handle<> inner) {
    base_.inner_ = inner;
    if (base_.cancel_token.cancel()) {
      return false;
    }
    write_       = {.inner = inner, .outer = base_.outer_};
    state_.write = &write_;

    scheduler.submit_write(base_.conn.fd(), &state_, use_awaiter_t{});
    cancel_.submit(scheduler, base_.cancel_token, inner, base_.outer_);
    return true;
  }

  auto await_resume() {
    if (cancel_.is_cancel_event(base_.cancel_token)) {
      cancel_.consume(base_.cancel_token);
      scheduler.remove_write(base_.conn.fd());
      cancel_.remove(scheduler, base_.cancel_token);
      return write_ret_t{operation_cancelled_error(), 0, op_status::CANCELLED};
    }
    cancel_.remove(scheduler, base_.cancel_token);

    scheduler.remove_write(base_.conn.fd());
    return try_write(base_.conn, base_.buf);
  }
};

template <typename SCHEDULER, typename CONNECTION_LIKE, typename BUFFER_VIEW,
          typename CANCEL_TOKEN = noop_cancel_token>
struct inner_send_timeout_awaiter {
  Base<CONNECTION_LIKE, BUFFER_VIEW, CANCEL_TOKEN, timeout_token> base_;
  SCHEDULER &scheduler;
  fd_ops write_state_{};
  write_op write_{};
  fd_ops timer_state_{};
  read_op timer_read_{};
  [[no_unique_address]] cancel_slot<CANCEL_TOKEN> cancel_;

  bool await_ready() noexcept { return false; }

  bool await_suspend(std::coroutine_handle<> inner) {
    base_.inner_ = inner;
    if (base_.cancel_token.cancel()) {
      return false;
    }
    write_             = {.inner = inner, .outer = base_.outer_};
    write_state_.fd    = base_.conn.fd();
    write_state_.write = &write_;
    scheduler.submit_write(base_.conn.fd(), &write_state_, use_awaiter_t{});

    timer_read_       = {.inner = inner, .outer = base_.outer_};
    timer_state_.fd   = TimerEvent::get_instance().fd();
    timer_state_.read = &timer_read_;
    auto &timer       = TimerEvent::get_instance();
    timer.set_timer(&timer_state_, base_.timer_token.delay);
    refresh_timer_registration(scheduler);
    cancel_.submit(scheduler, base_.cancel_token, inner, base_.outer_);
    return true;
  }

  write_ret_t await_resume() {
    auto &timer = TimerEvent::get_instance();
    if (cancel_.is_cancel_event(base_.cancel_token)) {
      cancel_.consume(base_.cancel_token);
      timer.cancel(&timer_read_);
      scheduler.remove_write(base_.conn.fd());
      cancel_.remove(scheduler, base_.cancel_token);
      refresh_timer_registration(scheduler);
      return {operation_cancelled_error(), 0, op_status::CANCELLED};
    }
    cancel_.remove(scheduler, base_.cancel_token);
    const bool timed_out =
        timer.is_current(&timer_read_) && timer.get_timer().count() == 0;
    if (timed_out) {
      timer.consume();
      timer.cancel(&timer_read_);
      scheduler.remove_write(base_.conn.fd());
      refresh_timer_registration(scheduler);
      return {std::make_error_code(std::errc::timed_out), 0,
              op_status::TIMEOUT};
    }

    timer.cancel(&timer_read_);
    refresh_timer_registration(scheduler);
    auto result = try_write(base_.conn, base_.buf);
    scheduler.remove_write(base_.conn.fd());
    return result;
  }
};

template <typename SCHEDULER, typename CONNECTION_LIKE, typename BUFFER_VIEW,
          typename CANCEL_TOKEN = noop_cancel_token>
Task<result_t>
async_write_impl(Base<CONNECTION_LIKE, BUFFER_VIEW, CANCEL_TOKEN> base_,
                 SCHEDULER &s) {
  size_t total_len = 0;
  while (1) {
    auto [ec, len, stat_] =
        co_await inner_send_awaiter<SCHEDULER, CONNECTION_LIKE, BUFFER_VIEW,
                                    CANCEL_TOKEN>{base_, s};
    total_len += len;
    if (stat_ == op_status::INPROGRESS) {
      continue;
    }
    co_return {ec, total_len};
  }
  co_return {};
}

template <typename SCHEDULER, typename CONNECTION_LIKE, typename BUFFER_VIEW,
          typename CANCEL_TOKEN = noop_cancel_token>
Task<result_t> async_write_timeout_impl(
    Base<CONNECTION_LIKE, BUFFER_VIEW, CANCEL_TOKEN, timeout_token> base_,
    SCHEDULER &s) {
  size_t total_len = 0;
  while (1) {
    auto [ec, len, stat_] =
        co_await inner_send_timeout_awaiter<SCHEDULER, CONNECTION_LIKE,
                                            BUFFER_VIEW, CANCEL_TOKEN>{base_,
                                                                       s};
    total_len += len;
    if (stat_ == op_status::INPROGRESS) {
      continue;
    }
    co_return {ec, total_len};
  }
  co_return {};
}

template <typename SCHEDULER, typename CONNECTION_LIKE, typename BUFFER_VIEW,
          typename CANCEL_TOKEN = noop_cancel_token>
auto async_write(SCHEDULER &scheduler, CONNECTION_LIKE conn,
                 BUFFER_VIEW &byte_views, use_awaiter_t,
                 CANCEL_TOKEN cancel_token = {}) {
  struct Awaiter {
    Base<CONNECTION_LIKE, BUFFER_VIEW, CANCEL_TOKEN> base_;
    SCHEDULER &s;
    Task<result_t> result{};
    write_operation_tag operation_kind() const noexcept { return {}; }
    bool await_ready() noexcept { return false; }

    bool await_suspend(std::coroutine_handle<> outer) noexcept {
      base_.outer_ = outer;
      result       = async_write_impl(base_, s);
      if (result.handle.done()) {
        return false;
      }
      return true;
    }

    result_t await_resume() noexcept { return result.result(); }
  };

  return Awaiter{
      .base_ = {.conn = conn, .buf = byte_views, .cancel_token = cancel_token},
      .s     = scheduler};
}

template <typename SCHEDULER, typename CONNECTION_LIKE, typename BUFFER_VIEW,
          typename CANCEL_TOKEN = noop_cancel_token>
auto async_write_timeout(SCHEDULER &scheduler, CONNECTION_LIKE conn,
                         BUFFER_VIEW &byte_views,
                         std::chrono::nanoseconds nanosec_, use_awaiter_t,
                         CANCEL_TOKEN cancel_token = {}) {
  struct Awaiter {
    Base<CONNECTION_LIKE, BUFFER_VIEW, CANCEL_TOKEN, timeout_token> base_;
    SCHEDULER &s;
    Task<result_t> result{};
    write_operation_tag operation_kind() const noexcept { return {}; }
    bool await_ready() noexcept { return false; }

    bool await_suspend(std::coroutine_handle<> outer) noexcept {
      base_.outer_ = outer;
      result       = async_write_timeout_impl(base_, s);
      if (result.handle.done()) {
        return false;
      }
      return true;
    }

    result_t await_resume() noexcept { return result.result(); }
  };

  return Awaiter{.base_ = {.conn         = conn,
                           .buf          = byte_views,
                           .cancel_token = cancel_token,
                           .timer_token  = timeout_token{nanosec_}},
                 .s     = scheduler};
}

} // namespace detail
} // namespace ASYNC_FRAME

#endif
