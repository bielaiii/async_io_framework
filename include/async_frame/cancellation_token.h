#ifndef CANCELLATION_TOKEN
#define CANCELLATION_TOKEN

#include <atomic>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <unistd.h>
#include <sys/eventfd.h>
template <typename CANCEL_TOKEN>
concept CANCEL_TOKEN_LIKE = requires(CANCEL_TOKEN token) { token.cancel(); };

struct noop_cancel_token {
    [[nodiscard]] constexpr bool cancel() noexcept { return false; };
    constexpr void throw_if_cancel() const {}
};

// the error code will be operation_canceled
// the byte_transformed will be zero
class cancel_token {
    struct shared_state {
        std::atomic_bool cancelled{false};
        int fd{-1};

        shared_state() : fd(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) {
            if (fd == -1) {
                throw std::system_error(errno, std::generic_category(),
                                        "eventfd");
            }
        }

        shared_state(const shared_state &)            = delete;
        shared_state &operator=(const shared_state &) = delete;

        ~shared_state() {
            if (fd != -1) {
                close(fd);
            }
        }
    };

    std::shared_ptr<shared_state> state_{std::make_shared<shared_state>()};

public:
    [[nodiscard]] bool cancel() const noexcept {
        return state_->cancelled.load(std::memory_order_acquire);
    }

    void request_cancel() const noexcept {
        bool expected = false;
        if (state_->cancelled.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel)) {
            uint64_t value = 1;
            (void)::write(state_->fd, &value, sizeof(value));
        }
    }

    [[nodiscard]] int fd() const noexcept { return state_->fd; }

    void consume() const noexcept {
        uint64_t value = 0;
        while (::read(state_->fd, &value, sizeof(value)) == -1 &&
               errno == EINTR) {
        }
    }

    void throw_if_cancel() const {
        throw std::runtime_error("task cancelled");
    }
};

#endif
