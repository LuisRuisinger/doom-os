#ifndef DOOM_OS_KERNEL_CORE_RESULT_HPP_
#define DOOM_OS_KERNEL_CORE_RESULT_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/debug/kpanic.hpp"

// =================================================================================================
// Result<T, E>
//
// Single point where the third-party Result is configured and pulled in, so the failure hooks
// are wired the same way everywhere. Without this the library traps with __builtin_trap() in
// freestanding builds, which costs the file, line and register dump that kpanic already gives.
//
// Include this header rather than <result/result.hpp> directly.
// =================================================================================================

#ifndef RESULT_ERROR
#    define RESULT_ERROR(message__) KPANIC(message__)
#endif

#include <result/result.hpp>

namespace kernel::core {

using lsr::Err;
using lsr::Ok;
using lsr::Result;

}  // namespace kernel::core

#endif  // DOOM_OS_KERNEL_CORE_RESULT_HPP_
