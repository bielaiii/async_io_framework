#ifndef OPERATION_HEADER
#define OPERATION_HEADER

#include <coroutine>
#include <cstddef>
#include <functional>
#include <memory>
#include <print>
#include <timer.h>
#include <utility>
#include "IOperation.h"
#include "base.h"
#include "buffer.h"
#include "cancellation_token.h"
#include "scheduler.h"
#include "scheduler_type_traits.h"
#include "task.h"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
namespace ASYNC_FRAME {
namespace detail {

template <typename T>
class dispose_op : public operation {
    void dispose() { delete this; }

public:
    void complete() noexcept override {

        static_cast<T *>(this)->complete();
        dispose();
    }
};

class accept_operation : public operation {
    std::coroutine_handle<> outer_handle;
    std::coroutine_handle<> inner_handle;

public:
    accept_operation(std::coroutine_handle<> inner_,
                     std::coroutine_handle<> outer) noexcept
        : outer_handle(outer), inner_handle(inner_)  {}
    void complete() noexcept override {
        if (inner_handle && !inner_handle.done()) {
            inner_handle.resume();
            outer_handle.resume();
        }
    };

    void resume_outer_if_needed() noexcept {
        if (outer_handle && !outer_handle.done()) {
            outer_handle.resume();
        }
    }
};

template <typename CONNECTION_LIKE, typename BUFFER,
          typename CANCEL_TOKEN = noop_cancel_token>
class coro_operation : public operation {
private:
    Base<CONNECTION_LIKE, BUFFER, CANCEL_TOKEN> base_;

public:
    coro_operation(Base<CONNECTION_LIKE, BUFFER, CANCEL_TOKEN> b_) noexcept
        : base_(b_) {}

    void complete() noexcept override {
        if (base_.cancel_token.cancel()) {
            base_.outer_.resume();
            return;
        }
        if (base_.inner_ && !base_.inner_.done()) {
            base_.inner_.resume();
        }

        if (base_.inner_.done()) {
            base_.outer_.resume();
        }
    };

    void resume_outer_if_needed() noexcept {
        if (base_.outer_ && !base_.outer_.done()) {
            base_.outer_.resume();
        }
    }
};

template <typename CONNECTION>
struct coro_connect_operation : public operation {
    handle_t outer_handle;
    handle_t inner_handle;

    coro_connect_operation(handle_t outer_, handle_t inner) noexcept
        : outer_handle(outer_), inner_handle(inner) {}

    void complete() noexcept override {
        /* if (inner_handle && !inner_handle.done()) {
            inner_handle.resume();
        } */
        //   std::print(stderr, "is done ? {}", outer_handle.done());
        outer_handle.resume();
        /*  if (inner_handle.done()) {
             outer_handle.resume();
         } */
    }
};

template <typename Handler>
class call_back_operation : public dispose_op<call_back_operation<Handler>> {
    int fd;
    DynamicBuffer &buf;
    Handler handle_;

public:
    call_back_operation(int f, DynamicBuffer &buf, Handler &&h) noexcept
        : fd(f), buf(buf), handle_(std::move(h)) {}
    void complete() noexcept override {

        // auto [ec, byte_transformed] = do_real_recv(fd, buf);
        // std::invoke(handle_, ec, byte_transformed);
    };
};

struct coro_resume : operation {
    std::coroutine_handle<> h_;

    void on_complete() noexcept { h_.resume(); }
};


} // namespace detail
} // namespace ASYNC_FRAME
#endif
