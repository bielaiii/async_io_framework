#ifndef TRY_CONNECT_HEADER
#define TRY_CONNECT_HEADER

#include <cerrno>
#include <netinet/in.h>

enum class connect_status { CONNECTED, ERROR, INPROGESS };

template <typename CONNECTION, typename FAMILY>
connect_status try_connect(CONNECTION conn, sockaddr *addr, size_t addr_len) {
    auto ret = ::connect(conn.fd(), addr, addr_len);
    if (ret == 0) {
        return connect_status::CONNECTED;
    }
    if (ret == -1) {
        if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
            return connect_status::INPROGESS;
        }
        return connect_status::ERROR;
    }
    return connect_status::ERROR;
}

#endif
