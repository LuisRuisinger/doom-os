#ifndef DOOM_OS_KERNEL_RUNTIME_NEW_HPP_
#define DOOM_OS_KERNEL_RUNTIME_NEW_HPP_

// =================================================================================================
// Cpp stdlib files
// =================================================================================================

// Placement new and its matching placement delete come from <new>, which the freestanding
// libstdc++ provides. Defining them here as well is a redefinition error in any translation
// unit that also reaches <new>.
#include <new>

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

#endif  // DOOM_OS_KERNEL_RUNTIME_NEW_HPP_