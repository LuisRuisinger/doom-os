#ifndef DOOM_OS_KERNEL_MM_DLMALLOC_CONFIG_H_
#define DOOM_OS_KERNEL_MM_DLMALLOC_CONFIG_H_

#include <stddef.h>
#include "kernel/runtime/memory.hpp"

/* Prefix all symbols with 'dl' (dlmalloc, dlfree, dlmemalign, etc.)
 * This completely avoids symbol collisions with standard libc malloc without
 * needing the mspace arena abstraction. */
#define USE_DL_PREFIX 1

#define HAVE_MORECORE        1
#define MORECORE             doom_os_kheap_morecore
#define MORECORE_CONTIGUOUS  0
#define MORECORE_CANNOT_TRIM 1

#define HAVE_MMAP   0
#define HAVE_MREMAP 0

/* Match frame granularity to page_size::SIZE_2M */
#define DEFAULT_GRANULARITY ((size_t)2U * 1024U * 1024U)

#define USE_LOCKS 0
#define FOOTERS   0

#define NO_MALLOC_STATS 1
#define MALLOC_FAILURE_ACTION
#define ABORT              doom_os_kheap_abort()
#define malloc_getpagesize ((size_t)4096U)

/* Freestanding toolchain lacks standard POSIX headers */
#define LACKS_ERRNO_H     1
#define LACKS_FCNTL_H     1
#define LACKS_SCHED_H     1
#define LACKS_STDLIB_H    1
#define LACKS_STRING_H    1
#define LACKS_STRINGS_H   1
#define LACKS_SYS_MMAN_H  1
#define LACKS_SYS_PARAM_H 1
#define LACKS_SYS_TYPES_H 1
#define LACKS_TIME_H      1
#define LACKS_UNISTD_H    1

#ifdef __cplusplus
extern "C" {
#endif

void *doom_os_kheap_morecore(ptrdiff_t increment);
void  doom_os_kheap_abort(void);

#ifdef __cplusplus
}
#endif

#endif /* DOOM_OS_KERNEL_MM_DLMALLOC_CONFIG_H_ */