#ifndef DOOM_OS_KERNEL_RUNTIME_MEMORY_HPP_
#define DOOM_OS_KERNEL_RUNTIME_MEMORY_HPP_

#include <stddef.h>

#ifdef __cplusplus
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
