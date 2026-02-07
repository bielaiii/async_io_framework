#ifndef DO_WRITE_HEADER
#define DO_WRITE_HEADER

#include "scheduler_type_traits.h"
#include <sys/socket.h>
#include <system_error>
namespace ASYNC_FRAME {
namespace detail {

using write_ret_t = std::tuple<std::error_code, size_t, op_status>;

bool stop_writing_again(std::error_code ec, ssize_t len) noexcept {
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
write_ret_t try_write(CONNCECTION conn, BUFFER &buf) {
    std::error_code ec;
    ssize_t byte_tranformed =
        ::send(conn.fd(), buf.seek(), buf.remain(), MSG_DONTWAIT);
    if (byte_tranformed == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return {ec, 0, op_status::INPROGRESS};
        }
        ec = std::error_code(errno, std::generic_category());
        return {ec, 0, op_status::ERROROUS};
    }

    if (byte_tranformed == 0) {
        return {ec, 0, op_status::SHUTDOWN};
    }
    buf += byte_tranformed;
    if (buf.done()) {
        return {ec, static_cast<size_t>(byte_tranformed), op_status::COMPLETED};
    }
    return {ec, static_cast<size_t>(byte_tranformed), op_status::INPROGRESS};
}
} // namespace detail
} // namespace ASYNC_FRAME

#endif
