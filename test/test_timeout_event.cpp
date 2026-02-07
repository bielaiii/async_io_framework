#include <print>
#include "async_connect.h"
#include "async_read.h"
#include "async_write.h"
#include "buffer.h"
#include "scheduler.h"
#include "scheduler_type_traits.h"
#include "with_timeout.h"
#include <type_traits>

using namespace std;
using namespace ASYNC_FRAME::detail;

Task<int> test_timerout_op(Scheduler &s) {
    auto [ec1, conn] = co_await async_connect(s, "127.0.0.1", 7899);

    if (ec1) {
        co_return {};
    }
    print("connect success");
    // conn.get_viewer();
    auto buf = DynamicBuffer(5);

    // auto [ec, byte_tranformed] = {1,1};
    // auto cw = async_read(s, conn.get_viewer(), buf, use_awaiter_t{});
    // static_assert(std::is_copy_constructible_v<decltype(cw.base_)>,"shoudl "
    // );
    // static_assert(std::is_copy_constructible_v<decltype(cw.result)>,"shoudl "
    // ); static_assert(std::is_copy_constructible_v<decltype(cw.s)>,"shoudl "
    // ); co_await with_timeout(
    //    async_read(s, conn.get_viewer(), buf, use_awaiter_t{}), 5000ns);
    /* if (!ec) {
        print("recv : {}", reinterpret_cast<char*>(buf.data()));
    } else {
        print(stderr, "err {}", ec.message());
    } */
    string hel = "hello,world";
    auto dd    = Buffer_View(hel.data(), hel.size());
    while (1) {
        auto [ecw, le] =
            co_await async_write(s, conn.get_viewer(), dd, use_awaiter_t{});
        if (ecw) {
            print("write erro  len {}", le);
        }
        auto [ec, ans] = co_await with_timeout(
            async_read(s, conn.get_viewer(), buf, use_awaiter_t{}), 2ns);

        if (!ec) {
            print("recv : {}", reinterpret_cast<char *>(buf.data()));
        } else {
            print(stderr, "err {}", ec.message());
        }

    }
}

int main() {
    Scheduler s{};
    test_timerout_op(s);
    s.running();
    // std::print("{}", h.result());
    return 0;
}