#ifndef TIMER_TASK_HEADER
#define TIMER_TASK_HEADER
#include <chrono>
#include <coroutine>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <format>
#include <print>
#include <queue>
#include <ratio>
#include <stdexcept>
#include <unistd.h>
#include <utility>
#include <vector>
#include "cancellation_token.h"
#include "connection.h"
#include "fd_object.h"
#include "scheduler_type_traits.h"
#include "single_operator.h"
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/timerfd.h>
namespace ASYNC_FRAME {
namespace detail {
using namespace std::chrono_literals;

class TimerEvent {
public:
    using time_unit_t   = std::chrono::nanoseconds;
    using time_point_t  = std::chrono::time_point<std::chrono::nanoseconds>;
    using steady_clock_ = std::chrono::steady_clock;

private:
    struct TimerTask {
        fd_ops state_;
        time_point_t time_point;
    };
    struct TimerTaskComp {
        bool operator()(const TimerTask &lhs, const TimerTask &rhs) {
            return lhs.time_point >= rhs.time_point;
        }
    };

    TimerTask cur;
    fdo fd_;
    std::priority_queue<TimerTask, std::vector<TimerTask>, TimerTaskComp>
        waiting_queue{};

    static fdo create_time_fd() {
        int temp_fd_ = timerfd_create(CLOCK_MONOTONIC, 0);
        if (temp_fd_ == -1) {
            auto err_msg =
                std::format("timerfd_create() : {}\n", strerror(errno));
            throw std::runtime_error(err_msg.c_str());
        }
        return fdo{temp_fd_};
    }

    TimerEvent() : fd_(create_time_fd()), waiting_queue() {};

public:
    TimerEvent(const TimerEvent &)            = delete;
    TimerEvent &operator=(const TimerEvent &) = delete;

    int fd() noexcept { return fd_.fd(); }

    static time_point_t to_time_point(time_unit_t delay) {
        return time_point_t{(delay + steady_clock_::now()).time_since_epoch()};
    }

    static TimerEvent &get_instance() {
        static TimerEvent instance = TimerEvent();
        return instance;
    }

    static struct timespec to_timespec(time_point_t time_) {
        auto second_ =
            std::chrono::time_point_cast<std::chrono::seconds>(time_);

        auto nsecond_ =
            std::chrono::time_point_cast<std::chrono::nanoseconds>(time_) -
            std::chrono::time_point_cast<std::chrono::nanoseconds>(second_);
        return {.tv_sec  = second_.time_since_epoch().count(),
                .tv_nsec = nsecond_.count()};
    }

    /* template <typename SCHEDULER>
    void set_next_timer(SCHEDULER &s) noexcept {
        auto &last_time        = waiting_queue.top();
        time_point_t delay_    = last_time.time_point;
        struct itimerspec utmr = {
            .it_interval = {},
            .it_value    = to_timespec(delay_),
        };
        state_ = last_time.state_;
        cur    = delay_;
        epoll_event ev{.events = std::to_underlying(register_type::EVENT_TIMER),
                       .data   = {.ptr = &state_}};

        // we use absolute time
        timerfd_settime(fd_.fd(), TFD_TIMER_ABSTIME, &utmr, nullptr);
    } */

    Connection_Viewer conn() noexcept { return Connection_Viewer(fd_.fd()); }

    

    void update_timer(time_point_t tp) {
        struct itimerspec utmr = {
            .it_interval = {},
            .it_value    = to_timespec(tp),
        };
        if (timerfd_settime(fd_.fd(), TFD_TIMER_ABSTIME, &utmr, nullptr) < 0) {
            ERR_MSG;
        }
    }

    void clear_timer() {
        struct itimerspec utmr {};
        if (timerfd_settime(fd_.fd(), 0, &utmr, nullptr) < 0) {
            ERR_MSG;
        }
    }

    void consume() noexcept {
        uint64_t expirations = 0;
        while (::read(fd_.fd(), &expirations, sizeof(expirations)) == -1 &&
               errno == EINTR) {
        }
    }

    // cur save the current setting, epoll resume with this address
    // queue.top() == cur

    TimerEvent &operator++(int) {
        if (waiting_queue.empty()) {
            cur = TimerTask{};
            clear_timer();
            return *this;
        }
        cur                    = waiting_queue.top();
        
        update_timer(cur.time_point);

        waiting_queue.pop();
        return *this;
    }

    [[nodiscard]] bool current_empty() const noexcept {
        return cur.time_point == time_point_t{};
    }

    [[nodiscard]] bool is_current(read_op *read) const noexcept {
        return cur.state_.read == read;
    }

    void cancel(read_op *read) {
        if (read == nullptr) {
            return;
        }
        if (cur.state_.read == read) {
            (*this)++;
            return;
        }

        std::vector<TimerTask> remaining;
        remaining.reserve(waiting_queue.size());
        while (!waiting_queue.empty()) {
            auto task = waiting_queue.top();
            waiting_queue.pop();
            if (task.state_.read != read) {
                remaining.push_back(task);
            }
        }
        waiting_queue =
            decltype(waiting_queue)(TimerTaskComp{}, std::move(remaining));
    }

    void set_timer(fd_ops *state_, time_unit_t delay) noexcept {
        auto final_time = to_time_point(delay);
        if (cur.time_point == time_point_t{}) {
            cur.time_point = final_time;
            cur.state_ = *state_;
            update_timer(final_time);
            return;
        }

        TimerTask task_{.state_ = *state_, .time_point = final_time};
        if (cur.time_point > final_time) {
            waiting_queue.push(cur);
            cur = task_;
            update_timer(final_time);
        } else {
            waiting_queue.push(task_);
        }

        return;
    }

    bool empty() noexcept { return waiting_queue.empty(); }

    /* template <typename SCHEDULER>
    int next_timer(SCHEDULER &s) {

        waiting_queue.pop();
        if (waiting_queue.empty()) {
            s.unregister_event(Connection_Viewer(fd()),
                               register_type::EVENT_TIMER, nullptr);
            return 0;
        }

        set_next_timer(s);

        return 1;
    } */

    std::chrono::nanoseconds get_timer() noexcept {
        struct itimerspec tv;
        timerfd_gettime(fd_.fd(), &tv);
        return std::chrono::nanoseconds(tv.it_value.tv_nsec +
                                        tv.it_value.tv_sec * std::nano::den);
    }

    TimerTask &current() { return cur; }
    ~TimerEvent() noexcept = default;
};

template <typename SCHEDULER>
void refresh_timer_registration(SCHEDULER &s) {
    auto &instance = TimerEvent::get_instance();
    if (instance.current_empty()) {
        s.remove_timer(instance.fd());
    } else {
        s.submit_timer(instance.fd(), &instance.current().state_,
                       use_awaiter_t{});
    }
}

} // namespace detail
} // namespace ASYNC_FRAME

#endif
