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
#include <unordered_map>
#include <unordered_set>
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
        std::uint64_t id{};
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
    std::uint64_t next_timer_id{1};
    std::unordered_map<read_op *, std::uint64_t> active_timer_ids{};
    std::unordered_set<std::uint64_t> cancelled_timer_ids{};

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

    [[nodiscard]] static bool empty_task(const TimerTask &task) noexcept {
        return task.time_point == time_point_t{};
    }

    void remember_active(const TimerTask &task) {
        if (task.state_.read != nullptr) {
            active_timer_ids[task.state_.read] = task.id;
        }
    }

    void forget_active(const TimerTask &task) {
        auto *read = task.state_.read;
        if (read == nullptr) {
            return;
        }
        auto it = active_timer_ids.find(read);
        if (it != active_timer_ids.end() && it->second == task.id) {
            active_timer_ids.erase(it);
        }
    }

    bool consume_cancelled(const TimerTask &task) {
        if (task.id == 0) {
            return false;
        }
        return cancelled_timer_ids.erase(task.id) != 0;
    }

    void activate_next_timer() {
        while (!waiting_queue.empty()) {
            auto task = waiting_queue.top();
            waiting_queue.pop();
            if (consume_cancelled(task)) {
                forget_active(task);
                continue;
            }
            cur = task;
            update_timer(cur.time_point);
            return;
        }

        cur = TimerTask{};
        clear_timer();
    }

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

    TimerEvent &operator++(int) {
        forget_active(cur);
        activate_next_timer();
        return *this;
    }

    void refresh_current_timer() {
        if (empty_task(cur) || consume_cancelled(cur)) {
            forget_active(cur);
            activate_next_timer();
        }
    }

    [[nodiscard]] bool current_empty() const noexcept {
        return empty_task(cur);
    }

    [[nodiscard]] bool is_current(read_op *read) const noexcept {
        return cur.state_.read == read;
    }

    void cancel(read_op *read) {
        if (read == nullptr) {
            return;
        }
        auto it = active_timer_ids.find(read);
        if (it == active_timer_ids.end()) {
            return;
        }
        cancelled_timer_ids.insert(it->second);
        active_timer_ids.erase(it);
    }

    void set_timer(fd_ops *state_, time_unit_t delay) noexcept {
        refresh_current_timer();
        auto final_time = to_time_point(delay);
        TimerTask task_{.state_ = *state_,
                        .time_point = final_time,
                        .id = next_timer_id++};
        remember_active(task_);
        if (cur.time_point == time_point_t{}) {
            cur = task_;
            update_timer(final_time);
            return;
        }

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
    instance.refresh_current_timer();
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
