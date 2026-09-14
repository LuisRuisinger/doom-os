#ifndef DOOM_OS_KERNEL_RUNTIME_MEMORY_HPP_
#define DOOM_OS_KERNEL_RUNTIME_MEMORY_HPP_

// =================================================================================================
// Freestanding libc memory functions
//
// Not an API to call by preference - these exist because the compiler emits calls to them whether
// or not anything asked. A struct copy or a zero-initialised array is enough. The definitions live
// in memory.cpp.
//
// Kept C-valid despite the extension, and it has to stay that way: malloc.c reaches these through
// kernel/mm/dlmalloc_config.h and is compiled as C, so a namespace or a template added here breaks
// that build rather than this one. The guards below are what let one file serve both.
//
// One file rather than a C and a C++ spelling, because there is one set of symbols. uk/ carries
// both spellings because the two describe genuinely different declarations - doom_os_driver_log
// against uk::services::log - with services_abi.cpp forwarding between them. There is nothing to
// forward to here.
// =================================================================================================

#include <stddef.h>

#ifdef __cplusplus
// memory.cpp defines these noexcept, and since C++17 an exception specification is part of the
// function type - a plain declaration would be a conflicting one rather than the same function.
// C has nothing to say on the subject and takes the empty expansion.
#    define DOOM_OS_MEMORY_NOTHROW noexcept
extern "C" {
#else
#    define DOOM_OS_MEMORY_NOTHROW
#endif

void *memset(void *dest, int value, size_t count) DOOM_OS_MEMORY_NOTHROW;
void *memcpy(void *dest, const void *src, size_t count) DOOM_OS_MEMORY_NOTHROW;
void *memmove(void *dest, const void *src, size_t count) DOOM_OS_MEMORY_NOTHROW;
int memcmp(const void *lhs, const void *rhs, size_t count) DOOM_OS_MEMORY_NOTHROW;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DOOM_OS_KERNEL_RUNTIME_MEMORY_HPP_
