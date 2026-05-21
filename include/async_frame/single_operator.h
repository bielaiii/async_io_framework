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

}
}
#endif
