#ifndef DOOM_OS_KERNEL_DEBUG_KPANIC_HPP_
#define DOOM_OS_KERNEL_DEBUG_KPANIC_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"

namespace kernel::debug {
using kernel::core::u64;

// =================================================================================================
// Kernel panic
// =================================================================================================

[[noreturn]] void kpanic(const char *message);

[[noreturn]] void kpanic_with_code(const char *message, u64 code);
}  // namespace kernel::debug

#endif  // DOOM_OS_KERNEL_DEBUG_KPANIC_HPP_