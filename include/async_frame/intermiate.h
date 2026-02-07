#ifndef INTERMEDIATE_HEADER
#define INTERMEDIATE_HEADER

#include "connector.hpp"
#include <cstring>
#include <stdexcept>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <type_traits>

namespace ASYNC_FRAME {
namespace detail {

struct IPV4_VERSION {};
struct IPV6_VERSION {};

template <typename IP_VERSION>
struct InterMediate {
    sockaddr_in sock_;
    InterMediate(const std::string &add, int port) {
        int ret;
        if constexpr (std::is_same_v<IP_VERSION, IPV4_VERSION>) {
            sock_.sin_family = AF_INET;
            sock_.sin_port   = htons(port);
            ret              = inet_pton(AF_INET, add.data(), &sock_.sin_addr);

        } else if (std::is_same_v<IP_VERSION, IPV6_VERSION>) {
            sock_.sin_family = AF_INET6;
            sock_.sin_port   = htons(port);
            ret              = inet_pton(AF_INET6, add.data(), &sock_.sin_addr);

        } else {
            static_assert(sizeof(char) != sizeof(char),
                          "unsuppoted protocal version");
        }

        if (ret == 1) {
            return;
        }
        if (ret == 0) {
            throw std::invalid_argument("invalid address");
        }
        {
            std::print(stderr, "inet_pton error: {}\n", strerror(errno));
            throw std::runtime_error("inet_pton failed");
        }
    };
};
} // namespace detail
} // namespace ASYNC_FRAME

#endif