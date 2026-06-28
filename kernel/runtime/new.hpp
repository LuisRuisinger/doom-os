#ifndef DOOM_OS_KERNEL_RUNTIME_NEW_HPP_
#define DOOM_OS_KERNEL_RUNTIME_NEW_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"

// =================================================================================================
// Global allocation operators
// =================================================================================================

void *operator new(kernel::core::usize size);
void *operator new[](kernel::core::usize size);

void operator delete(void *ptr) noexcept;
void operator delete(void *ptr, kernel::core::usize size) noexcept;

void operator delete[](void *ptr) noexcept;
void operator delete[](void *ptr, kernel::core::usize size) noexcept;

// =================================================================================================
// Placement new
// =================================================================================================

inline void *operator new(kernel::core::usize, void *place) noexcept { return place; }

inline void *operator new[](kernel::core::usize, void *place) noexcept { return place; }

inline void operator delete(void *, void *) noexcept {}
inline void operator delete[](void *, void *) noexcept {}

#endif  // DOOM_OS_KERNEL_RUNTIME_NEW_HPP_