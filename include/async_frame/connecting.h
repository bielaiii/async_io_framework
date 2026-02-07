#ifndef CONNECTING_HEADER
#define CONNECTING_HEADER

#include "connector.hpp"
#include <connection.h>
#include <cstddef>
#include <cstring>
#include <format>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <type_traits>
#include <fcntl.h>

namespace ASYNC_FRAME {
namespace detail {

#define MAX_LISTEN_SOCKET 1024

template <typename PROTOCAL>
struct PROTOCAL_TYPE {};

struct IPV4 {
    static constexpr int value = AF_INET;
};
struct IPV6 {
    static constexpr int value = AF_INET6;
};

template <typename TYPE_>
struct is_legal_PROTOCAL_Type : std::false_type {
    static_assert(
        sizeof(TYPE_) != sizeof(TYPE_),
        "You can only use IPV4 or IPV6 type for template FAMILY_TRAIT");
};

template <>
struct is_legal_PROTOCAL_Type<IPV4> : std::true_type {};

template <>
struct is_legal_PROTOCAL_Type<IPV6> : std::true_type {};

template <typename FAMILY_TRAIT, typename sock_addr_t>
bool ip_to_in_addr_t(const char *ip, sock_addr_t &addr) {
    static_assert(is_legal_PROTOCAL_Type<FAMILY_TRAIT>::value,
                  "USE IPV4 or IPV6 instead");
    int res = ::inet_pton(FAMILY_TRAIT::value, ip, &addr);
    if (res == 1) {
        return true;
    }
    if (res == 0) {
        throw std::invalid_argument("invalid ip address");
    }
    auto err_str = std::format("inet_pton() : {}", strerror(errno));
    throw std::runtime_error(err_str.c_str());

    return true;
}

template <typename FAMILY_VERSION>
struct FamilyTraits {};

template <>
struct FamilyTraits<IPV4> {
    using Type                    = struct sockaddr_in;
    static constexpr size_t len   = sizeof(Type);
    static constexpr size_t value = sizeof(Type);
    static constexpr bool parse(Type &addr, int port, const char *ip = "") {
        std::memset(&addr, 0, sizeof(Type));
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);
        if (ip == "") {
            addr.sin_addr.s_addr = INADDR_ANY;
            return true;
        }

        ip_to_in_addr_t<IPV4>(ip, addr.sin_addr.s_addr);
        return true;
    }
};

template <>
struct FamilyTraits<IPV6> {
    using Type                    = struct sockaddr_in6;
    static constexpr size_t len   = sizeof(Type);
    static constexpr size_t value = sizeof(Type);
    static constexpr bool parse(Type &addr, int port, const char *ip = "") {
        std::memset(&addr, 0, sizeof(Type));
        addr.sin6_family = AF_INET6;
        addr.sin6_port   = htons(port);
        if (ip == "") {
            addr.sin6_addr = IN6ADDR_ANY_INIT;
            return true;
        }

        ip_to_in_addr_t<IPV6>(ip, addr.sin6_addr);
        return 1;
    }
};

struct TCP {
    static constexpr int value = SOCK_STREAM | SOCK_NONBLOCK;
};
struct UDP {
    static constexpr int value = SOCK_DGRAM | SOCK_NONBLOCK;
};

struct CLENT_ROLE {};
struct SERVER_ROLE {};

template <typename FAMILY, typename PROTOCAL, typename ROLE>
struct ConnectionBuilder {

    /* static_assert(std::is_same_v<FAMILY, IPV4> || std::is_same_v<FAMILY,
    IPV4>, "FAMILY type can only be IPV4 or IPV6");

    static_assert(std::is_same_v<PROTOCAL, TCP_PROTOCAL> ||
                      std::is_same_v<PROTOCAL, UDP_PROTOCAL>,
                  "PROTOCAL can only be TCP_PROTOCAL or UDP_PROTOCAL");

    static_assert(std::is_same_v<ROLE, CLENT_ROLE> ||
                      std::is_same_v<ROLE, SERVER_ROLE>,
                  "ROLE can only be CLENT_ROLE or SERVER_ROLE"); */

    using Addr = typename FamilyTraits<FAMILY>::Type;

    static int create_sock(const char *ip, int port) {
        int fd = socket(FAMILY::value, PROTOCAL::value, 0);

        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

        using addr_type = FamilyTraits<FAMILY>::Type;

        addr_type addr;

        FamilyTraits<FAMILY>::parse(addr, ip, port);

        if (fd < 0) {
            throw std::runtime_error("socket() failed");
        }

        if (::connect(fd, addr, sizeof(addr))) {
        }

        return fd;
    }

    static int set_reuse_port(int fd) {
        int op = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &op, sizeof(op));
        return 1;
    }
    static int set_reuse_addr(int fd) {
        int op = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(op));
        return 1;
    }

    static Connection build(std::string_view ip, int port);
};

template <typename FAMILY, typename PROTOCAL>
static int create_sock(const char *ip, int port) {

    int fd = socket(FAMILY::value, PROTOCAL::value | SOCK_NONBLOCK, 0);
    if (fd < 0) {
        throw std::runtime_error("socket() failed");
    }
    using addr_type = FamilyTraits<FAMILY>::Type;

    addr_type addr;

    FamilyTraits<FAMILY>::parse(addr, port, ip);

    return fd;
}

template <typename FAMILY, typename PROTOCAL>
struct ConnectionBuilder<FAMILY, PROTOCAL, SERVER_ROLE> {
    static auto build(int port, const char *ip = "") -> decltype(1) {
        int fd          = create_sock<FAMILY, PROTOCAL>(ip, port);
        using addr_type = FamilyTraits<FAMILY>::Type;

        addr_type addr;

        FamilyTraits<FAMILY>::parse(addr, ip, port);

        if (::bind(fd, addr, sizeof(addr_type)) == -1) {
            auto err_msg = std::format("bind() : {}", strerror(errno));
            throw std::runtime_error(err_msg.c_str());
        }

        if (::listen(fd, MAX_LISTEN_SOCKET) == -1) {
            auto err_msg = std::format("listen() : {}", strerror(errno));
            throw std::runtime_error(err_msg.c_str());
        }
        return fd;
    }
};

template <typename FAMILY, typename PROTOCAL>
struct ConnectionBuilder<FAMILY, PROTOCAL, CLENT_ROLE> {
    using addr_t = typename FamilyTraits<FAMILY>::Type;
    static auto build(int port, const char *ip) {

        int fd = create_sock<FAMILY, PROTOCAL>(ip, port);

        addr_t addr;

        FamilyTraits<FAMILY>::parse(addr, port, ip);

        return std::make_pair(Connection_Viewer(fd), addr);
    }
};

/* default ip address = INADDR_ANY or IN6ADDR_ANY */
template <typename FAMILY, typename PROTOCAL>
Connection BuilderServer(int port, char *ip = "") {
    int fd = ConnectionBuilder<FAMILY, PROTOCAL, SERVER_ROLE>::build(ip, port);
    return std::move(Connection{fd});
}

template <typename FAMILY, typename PROTOCAL>
auto BuilderClient(int port, const char *ip) {

    return ConnectionBuilder<FAMILY, PROTOCAL, CLENT_ROLE>::build(port, ip);
}

} // namespace detail
} // namespace ASYNC_FRAME
#endif
