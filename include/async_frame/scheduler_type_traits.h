#ifndef SCHEDULER_TYPE_TRAITS_HEADER
#define SCHEDULER_TYPE_TRAITS_HEADER

#include <coroutine>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <sys/epoll.h>
#include <system_error>
#include <type_traits>
struct pretend_callable {
    void operator()();
    ;
};

// 新的实现：接受一个字符串（如 __func__）
#define ERR_MSG_IMPL(func_str)                                                 \
    do {                                                                       \
        auto msg = std::format("{}() : {}", func_str, strerror(errno));         \
        throw std::runtime_error(msg);                                         \
    } while (0)

// 直接传入 __func__（它本身就是 const char[]）
#define ERR_MSG ERR_MSG_IMPL(__func__)



template <typename T, typename = int>
struct is_scheduler : std::false_type {};

template <typename T>
struct is_scheduler<
    T, std::void_t<decltype(std::declval<T>().register_read(
           std::declval<int>(), std::declval<pretend_callable &&>()))>>
    : std::true_type {};

struct use_awaiter_t {};
struct use_timer_t {};
struct use_callback_t {};

enum class register_type : uint32_t {
    EMTPY_EVENT   = 0,
    EVENT_READ    = EPOLLIN,
    EVENT_ACCEPT  = EPOLLIN,
    EVENT_WRITE   = EPOLLOUT,
    EVENT_CONNECT = EPOLLOUT,
    EVENT_TIMER   = EPOLLIN | EPOLLET,

};

using buf_view = std::span<std::byte>;
using result_t = std::pair<std::error_code, size_t>;

using share_handle = std::coroutine_handle<>;
using share_result = std::shared_ptr<result_t>;

using handle_t = std::coroutine_handle<>;

template <typename SCHEDULER>
concept is_scheduler_v = requires(SCHEDULER s) { s.running(); };

template <typename CONNECTION_LIKE>
concept is_connection_v = requires(CONNECTION_LIKE conn) { conn.fd(); };

template <typename BUFFER>
concept is_buffer_v = requires(BUFFER buf) {
    buf.data();
    buf.size();
};

enum class op_status {
    ERROROUS,
    INPROGRESS,
    CANCELLED,
    SHUTDOWN, // socket closed
    COMPLETED,
    TIMEOUT
};

#endif
