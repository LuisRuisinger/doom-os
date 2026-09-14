#ifndef DOOM_OS_KERNEL_CORE_RESULT_HPP_
#define DOOM_OS_KERNEL_CORE_RESULT_HPP_

#include "kernel/debug/kpanic.hpp"

#ifndef RESULT_ERROR
#    define RESULT_ERROR(message__) KPANIC(message__)
#endif

#include <result/result.hpp>

namespace kernel::core {

using lsr::result::Err;
using lsr::result::Ok;
using lsr::result::Result;

template <typename T, typename... Args>
[[nodiscard]] auto Ok(Args &&...args)
{
    return lsr::result::wrapper::Ok<T>(lsr::result::detail::in_place,
                                       static_cast<Args &&>(args)...);
}

template <typename E, typename... Args>
[[nodiscard]] auto Err(Args &&...args)
{
    return lsr::result::wrapper::Err<E>(lsr::result::detail::in_place,
                                        static_cast<Args &&>(args)...);
}

}  // namespace kernel::core

#define KTRY(...)                                                                   \
    __extension__({                                                                 \
        auto &&ktry_result_ = (__VA_ARGS__);                                        \
        if (!ktry_result_.is_ok())                                                  \
            return ::kernel::core::Err(                                             \
                static_cast<decltype(ktry_result_) &&>(ktry_result_).unwrap_err()); \
        static_cast<decltype(ktry_result_) &&>(ktry_result_).unwrap();              \
    })

#endif  // DOOM_OS_KERNEL_CORE_RESULT_HPP_
