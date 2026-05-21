#ifndef DROP_LAST_TEMPLATE_ARGUMENT
#define DROP_LAST_TEMPLATE_ARGUMENT

#include <tuple>

namespace ASYNC_FRAME {
namespace detail {

template <typename NEW_CANCEL, template <typename...> class OP,
          typename... Args>
struct override_cancel {
    using type = OP<Args..., NEW_CANCEL>;
};

template <typename NEW_CANCEL, template <typename...> class OP,
          typename... Args>
using override_cancel_t =
    typename override_cancel<NEW_CANCEL, OP, Args...>::type;

} // namespace detail
} // namespace ASYNC_FRAME

#endif
