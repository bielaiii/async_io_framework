#include <cstddef>
#include "async_connect.h"
#include "async_read.h"
#include "async_write.h"
#include "buffer.h"
#include "connecting.h"

using namespace std;

using namespace ASYNC_FRAME::detail;

Task<int> test_connect(Scheduler &s) {

    auto [ec, new_fd] = co_await async_connect(s, "127.0.0.1", 7899);
    if (ec) {
        print(stderr, "Errno : {} : {}", errno, ec.message());
        co_return {};
    }
    string str = "hello, world\n";
    auto str_view = Buffer_View(str.data(), str.size());

    DynamicBuffer db(str.size());
    while (1) {
        auto [wec, wlen] = co_await async_write(s, new_fd.get_viewer(), str_view , use_awaiter_t{});
        if (wec) {
            print("async_write() : {}", wec.message());
            co_return {};
        }
        print(stderr, "send {} bytes\n", wlen);

        auto [rec, rlen] = co_await async_read(s, new_fd.get_viewer(), db, use_awaiter_t{});
        if (rec) {
            print("async_read() : {}", rec.message());
            co_return {};
        }
        print(stderr, "recv {} bytes", wlen);
        str_view.clear();
        db.clear();
    }
    co_return {};
}

int main() {

    auto make_socket = MakeSocket<TCP_PROTOCAL>{};
    auto s           = Scheduler{};
    using namespace ASYNC_FRAME::detail;

    auto e = test_connect(s);
    s.running();
    return 0;
}
