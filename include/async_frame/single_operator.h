#ifndef SINGLE_OPERATION
#define SINGLE_OPERATION
#include "cancellation_token.h"
#include "scheduler_type_traits.h"
#include"base.h"
#include<atomic>
namespace ASYNC_FRAME {
namespace detail {

/* 
template<typename CANCEL_TOKEN>
struct derived_read_op : I_read_op {
    Bundle<CANCEL_TOKEN> bu;
    handle_t most_outer;
    std::atomic<int> op_complete;
    void complete() noexcept override {
        if (bu.cancel_token.cancel()) {
            bu.inner_.destroy();
            bu.outer_.resume();
            return;
        }
        if (bu.inner && !bu.inner.done()) {
            bu.inner.resume();
        }
        if (bu.inner.done()) {
            bu.outer.resume();
        }
    }
}; */


struct read_op {
    handle_t inner{};
    handle_t outer{};

    void complete() noexcept {
        auto inner_handle = inner;
        auto outer_handle = outer;
        if (inner_handle && !inner_handle.done()) {
            inner_handle.resume();
        }
        if (inner_handle && inner_handle.done() && outer_handle &&
            !outer_handle.done()) {
            outer_handle.resume();
        }
    }
};

struct write_op {
    handle_t inner{};
    handle_t outer{};

    void complete() noexcept {
        auto inner_handle = inner;
        auto outer_handle = outer;
        if (inner_handle && !inner_handle.done()) {
            inner_handle.resume();
        }
        if (inner_handle && inner_handle.done() && outer_handle &&
            !outer_handle.done()) {
            outer_handle.resume();
        }
    }
};



struct fd_ops {
    int fd;
    write_op *write{};
    read_op *read{};
};

template <typename CANCEL_TOKEN>
struct cancel_slot {
    fd_ops state{};
    read_op read{};

    template <typename SCHEDULER>
    void submit(SCHEDULER &scheduler, const CANCEL_TOKEN &token,
                handle_t inner, handle_t outer) noexcept {
        if (token.cancel()) {
            return;
        }
        read       = {.inner = inner, .outer = outer};
        state.fd   = token.fd();
        state.read = &read;
        scheduler.submit_cancel(token.fd(), &state, use_awaiter_t{});
    }

    template <typename SCHEDULER>
    void remove(SCHEDULER &scheduler, const CANCEL_TOKEN &token) noexcept {
        scheduler.remove_cancel(token.fd());
    }

    bool is_cancel_event(const CANCEL_TOKEN &token) const noexcept {
        return token.cancel();
    }

    void consume(const CANCEL_TOKEN &token) const noexcept { token.consume(); }
};

template <>
struct cancel_slot<noop_cancel_token> {
    template <typename SCHEDULER>
    void submit(SCHEDULER &, const noop_cancel_token &, handle_t,
                handle_t) noexcept {}

    template <typename SCHEDULER>
    void remove(SCHEDULER &, const noop_cancel_token &) noexcept {}

    bool is_cancel_event(const noop_cancel_token &) const noexcept {
        return false;
    }

    void consume(const noop_cancel_token &) const noexcept {}
};

}
}
#endif
