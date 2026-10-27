#include "kernel/mm/kheap.hpp"

#include <stddef.h>

#include "kernel/core/cast.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/mm/page.hpp"
#include "kernel/mm/vmm.hpp"
#include "kernel/runtime/memory.hpp"
#include "kernel/sync/spinlock.hpp"

namespace {

using kernel::core::uptr;
using kernel::core::usize;
using kernel::mm::page_prot;

namespace vmm = kernel::mm::vmm;

constexpr usize NATURAL_ALIGNMENT = 2 * sizeof(void *);

constinit bool         m_heap_ready = false;
kernel::sync::spinlock m_lock{};

void *heap_mmap(usize bytes)
{
    return vmm::alloc(bytes, page_prot::KERNEL_DATA).unwrap_or((~uptr{0}) as(void *));
}

int heap_munmap(void *ptr, usize bytes)
{
    return vmm::free(ptr, bytes).is_ok() ? 0 : -1;
}

[[noreturn]] void corrupted()
{
    KPANIC("kernel heap detected a corrupted chunk");
}

}  // namespace

#define DLMALLOC_EXPORT        static
#define USE_DL_PREFIX          1
#define HAVE_MORECORE          0
#define HAVE_MMAP              1
#define MMAP(s)                heap_mmap(s)
#define DIRECT_MMAP(s)         heap_mmap(s)
#define MUNMAP(a, s)           heap_munmap((a), (s))
#define MMAP_CLEARS            0
#define HAVE_MREMAP            0
#define DEFAULT_GRANULARITY    (2U * 1024U * 1024U)
#define DEFAULT_MMAP_THRESHOLD (256U * 1024U)
#define USE_LOCKS              0
#define NO_MALLOC_STATS        1
#define MALLOC_FAILURE_ACTION
#define ABORT              corrupted()
#define malloc_getpagesize 4096U
#define EINVAL             22
#define ENOMEM             12
#define LACKS_ERRNO_H      1
#define LACKS_FCNTL_H      1
#define LACKS_SCHED_H      1
#define LACKS_STDLIB_H     1
#define LACKS_STRING_H     1
#define LACKS_STRINGS_H    1
#define LACKS_SYS_MMAN_H   1
#define LACKS_SYS_PARAM_H  1
#define LACKS_SYS_TYPES_H  1
#define LACKS_TIME_H       1
#define LACKS_UNISTD_H     1

extern "C" {

static size_t dlmalloc_usable_size(void *);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#if defined(__clang__)
#    pragma clang diagnostic ignored "-Wgnu-null-pointer-arithmetic"
#endif
#include <malloc.c>
#pragma GCC diagnostic pop

namespace kernel::mm::kheap {

void *alloc(usize bytes, usize alignment)
{
    if (!m_heap_ready)
        KPANIC("kernel heap used before KHEAP init");

    if (bytes == 0)
        return nullptr;

    kernel::sync::spinlock_guard guard(m_lock);

    if (alignment <= NATURAL_ALIGNMENT)
        return dlmalloc(bytes);

    return dlmemalign(alignment, bytes);
}

void free(void *ptr)
{
    if (ptr == nullptr)
        return;

    if (!m_heap_ready)
        KPANIC("kernel heap freed before KHEAP init");

    kernel::sync::spinlock_guard guard(m_lock);

    dlfree(ptr);
}

kernel::init::init_result component::init_heap()
{
    m_heap_ready = true;

    return kernel::core::Ok();
}

}  // namespace kernel::mm::kheap
