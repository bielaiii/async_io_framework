#include "async_connect.h"

using namespace std;

using namespace ASYNC_FRAME::detail;

Task<int> test_connect(Scheduler &s) {

    auto [ec, new_fd] = co_await async_connect(s, "127.0.0.1", 7899);
    if (ec) {
        print(stderr, "Errno : {} : {}", errno, ec.message());
        co_return {};
    }
    println("connect success");
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
