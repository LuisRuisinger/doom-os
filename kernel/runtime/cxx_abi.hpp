#ifndef DOOM_OS_KERNEL_RUNTIME_CXX_ABI_HPP_
#define DOOM_OS_KERNEL_RUNTIME_CXX_ABI_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"

namespace kernel::runtime {

using kernel::core::i32;

// =================================================================================================
// C++ ABI destructor registration
// =================================================================================================

using cxx_destructor = void (*)(void *);

i32 register_cxx_destructor(cxx_destructor destructor, void *object, void *dso) noexcept;

void finalize_cxx_destructors(void *dso) noexcept;

}  // namespace kernel::runtime

// =================================================================================================
// C++ ABI symbols
// =================================================================================================

extern "C" {

extern void *__dso_handle;

[[noreturn]] void __cxa_pure_virtual() noexcept;
[[noreturn]] void __cxa_deleted_virtual() noexcept;

int __cxa_atexit(void (*destructor)(void *), void *object, void *dso) noexcept;
void __cxa_finalize(void *dso) noexcept;

int __cxa_guard_acquire(kernel::core::u64 *guard) noexcept;
void __cxa_guard_release(kernel::core::u64 *guard) noexcept;
void __cxa_guard_abort(kernel::core::u64 *guard) noexcept;

}  // extern "C"

#endif  // DOOM_OS_KERNEL_RUNTIME_CXX_ABI_HPP_