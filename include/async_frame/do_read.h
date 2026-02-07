#ifndef DO_READ_HEADER
#define DO_READ_HEADER

#include <cerrno>
#include <unistd.h>
#include <utility>
#include "connection.h"
#include "scheduler_type_traits.h"
#include <sys/socket.h>
#include <system_error>
namespace ASYNC_FRAME {
namespace detail {

using read_ret_t = std::tuple<std::error_code, size_t>;

bool stop_reading_again(std::error_code ec, ssize_t len) noexcept {
    if (len == -1) {
        if (ec) {
            return true;
        }
        return false;
    }
    if (len == 0) {
        return true;
    }
    return !!ec;
}

template <typename CONNCECTION, typename BUFFER>
    requires is_connection_v<CONNCECTION> && is_buffer_v<BUFFER>
read_ret_t try_read(CONNCECTION conn, BUFFER &buf) noexcept {
    auto bytes_transferred = ::recv(conn.fd(), buf.seek(), buf.remain(), 0);
    std::error_code ec{};
    if (bytes_transferred == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return {ec, 0};
        }
        return {std::error_code(errno, std::generic_category()), 0};
    }
    if (bytes_transferred == 0) {
        return {ec, 0};
    }
    buf += bytes_transferred;
    if (buf.remain() == 0) {
        return {ec, bytes_transferred};
    }
    return {ec, bytes_transferred};
}


} // namespace detail
} // namespace ASYNC_FRAME

#endif