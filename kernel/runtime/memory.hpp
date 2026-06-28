#ifndef DOOM_OS_KERNEL_RUNTIME_MEMORY_HPP_
#define DOOM_OS_KERNEL_RUNTIME_MEMORY_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"

namespace kernel::runtime {

using kernel::core::i32;
using kernel::core::usize;

// =================================================================================================
// Memory functionality
// =================================================================================================

void *set(void *dest, i32 value, usize count) noexcept;
void *copy(void *dest, const void *src, usize count) noexcept;
void *move(void *dest, const void *src, usize count) noexcept;
i32   compare(const void *lhs, const void *rhs, usize count) noexcept;

}  // namespace kernel::runtime

// =================================================================================================
// C ABI wrappers
// =================================================================================================

extern "C" void *memset(void *dest, kernel::core::i32 value, kernel::core::usize count) noexcept;
extern "C" void *memcpy(void *dest, const void *src, kernel::core::usize count) noexcept;
extern "C" void *memmove(void *dest, const void *src, kernel::core::usize count) noexcept;
extern "C" kernel::core::i32 memcmp(const void *lhs, const void *rhs,
                                    kernel::core::usize count) noexcept;

#endif  // DOOM_OS_KERNEL_RUNTIME_MEMORY_HPP_