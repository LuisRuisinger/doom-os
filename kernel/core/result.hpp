#ifndef DOOM_OS_KERNEL_CORE_RESULT_HPP_
#define DOOM_OS_KERNEL_CORE_RESULT_HPP_

#include "kernel/core/cast.hpp"
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
    return lsr::result::wrapper::Ok<T>(lsr::result::detail::in_place, args as(Args &&)...);
}

template <typename E, typename... Args>
[[nodiscard]] auto Err(Args &&...args)
{
    return lsr::result::wrapper::Err<E>(lsr::result::detail::in_place, args as(Args &&)...);
}

}  // namespace kernel::core

#define KTRY(...)                                                                                  \
    __extension__({                                                                                \
        auto &&ktry_result_ = (__VA_ARGS__);                                                       \
        if (!ktry_result_.is_ok())                                                                 \
            return ::kernel::core::Err((ktry_result_ as(decltype(ktry_result_) &&)).unwrap_err()); \
        (ktry_result_ as(decltype(ktry_result_) &&)).unwrap();                                     \
    })

#endif  // DOOM_OS_KERNEL_CORE_RESULT_HPP_
