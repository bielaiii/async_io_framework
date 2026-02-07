#include "epoll_manager.h"
#include "scheduler.h"
#include "timer.h"
#include "wait_for.h"
using namespace std;
using namespace ASYNC_FRAME::detail;

Task<int> sleep_one_second(Scheduler &s) {
    co_await wait_for(s, 1s);
    std::println("func1 sleep for 1s");
    co_await wait_for(s, 2s);
    std::println("func1 sleep for 2s");
    co_await wait_for(s, 3s);
    std::println("func1 sleep for 3s");
    co_await wait_for(s, 4s);
    std::println("func1 sleep for 4s");
    co_return {};
}

Task<int> sleet_two_second(Scheduler &s) {
    co_await wait_for(s, 4s);
    std::println("func2 sleep for 4s");
    co_await wait_for(s, 3s);
    std::println("func2 sleep for 3s");
    co_await wait_for(s, 2s);
    std::println("func2 sleep for 2s");
    co_await wait_for(s, 1s);
    std::println("func2 sleep for 1s");
    co_return {};
}

int main() {
    Scheduler s{};
    auto h1 = sleep_one_second(s);
    auto h2 = sleet_two_second(s);
    s.running();
    // std::print("{}", h.result());
    return 0;
}