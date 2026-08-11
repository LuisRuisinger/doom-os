#ifndef DOOM_OS_KERNEL_RUNTIME_MEMORY_HPP_
#define DOOM_OS_KERNEL_RUNTIME_MEMORY_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"

// =================================================================================================
// Freestanding libc memory functions
//
// Not an API to call by preference - these exist because the compiler assumes they do. Include
// this header when you need the declarations in scope; the definitions live in memory.cpp.
// =================================================================================================

extern "C" void *memset(void *dest, kernel::core::i32 value, kernel::core::usize count) noexcept;
extern "C" void *memcpy(void *dest, const void *src, kernel::core::usize count) noexcept;
extern "C" void *memmove(void *dest, const void *src, kernel::core::usize count) noexcept;
extern "C" kernel::core::i32 memcmp(const void *lhs, const void *rhs,
                                    kernel::core::usize count) noexcept;

#endif  // DOOM_OS_KERNEL_RUNTIME_MEMORY_HPP_
