

#include "connector.hpp"
#include <exception>
#include <future>
#include <iostream>
#include <print>
#include <stdexcept>
#include <system_error>
using namespace std;
using namespace ASYNC_FRAME::detail;
int main() {
   using namespace ASYNC_FRAME::detail;
    using IO_CTX = ASYNC_FRAME::detail::IO_CONTEXT;
    auto make_socket = MakeSocket<TCP_PROTOCAL>{};
 //   auto io_ = std::make_shared<IO_CTX>();
 //   auto client_ = Connector::ConnectingNow(make_socket, "127.0.0.1", 8080, io_);

    return 0;
}