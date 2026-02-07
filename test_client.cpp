
#include "async_frame.hpp"
#include "connector.hpp"
#include <coroutine>
#include <cstdio>
#include <memory>
#include <print>
#include <thread>
#include "async_accept.h"
#include "async_connect.h"
#include "async_read.h"
#include "async_write.h"
#include "buffer.h"
#include "connecting.h"
#include "connection.h"
#include "iostream"
#include "scheduler.h"
#include "scheduler_type_traits.h"
#include <string_view>
#include <sys/socket.h>

using namespace std;

using namespace ASYNC_FRAME::detail;

Task<int> test_connect(Scheduler &s) {

    //  auto [ec, new_fd] = co_await async_connect(s, "127.0.0.1", 7899);
    //  if (ec) {
    //      print(stderr, "Errno : {} : {}", errno, ec.message());
    //      co_return {};
    //  }
    //  co_return {};
    int remote_port = 7899;
    int sock        = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port   = htons(remote_port);
    if (inet_pton(AF_INET, "127.0.0.1", &remote.sin_addr) <= 0) {
        co_return {};
    }

    if (connect(sock, (sockaddr *)&remote, sizeof(remote)) < 0) {
        perror("connect() failed");
        co_return {};
    }
    std::print(stderr, "connect success\n");
    auto new_fd     = Connection(sock);
    std::string str = "hello";
    Buffer_View ww(str.data(), str.size());
    auto recbuf = DynamicBuffer(5);
    while (1) {
        auto [wec, wlen] =
            co_await async_write(s, new_fd.get_viewer(), ww, use_awaiter_t{});

        if (wec) {
            std::print("send failed errno {} : {}", wec.value(), wec.message());
        } else {
            println("send {} bytes", wlen);
        }
        auto [rec, rlen] = co_await async_read(s, new_fd.get_viewer(), recbuf,
                                               use_awaiter_t{});
        if (rec) {
            std::print("recv failed errno {} : {}", wec.value(), wec.message());
            break;
            ;
        } else {
            println("recv {} bytes", wlen);
        }
    }
}

int main() {

    auto make_socket = MakeSocket<TCP_PROTOCAL>{};
    auto s           = Scheduler{};
    using namespace ASYNC_FRAME::detail;

    // auto can = std::make_shared<Connection>(-1);
    test_connect(s);
    s.running();
    return 0;
}