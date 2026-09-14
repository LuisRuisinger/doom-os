#include "kernel/mm/kheap.hpp"

#include <stddef.h>

#include "kernel/core/cast.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/mm/page.hpp"
#include "kernel/mm/pmm.hpp"
#include "kernel/mm/vmm.hpp"
#include "kernel/runtime/memory.hpp"

namespace {

using kernel::core::paddr_t;
using kernel::core::u8;
using kernel::core::uptr;
using kernel::core::usize;
using kernel::mm::page_size;

namespace pmm = kernel::mm::pmm;
namespace vmm = kernel::mm::vmm;

constexpr usize FRAME_BYTES = kernel::mm::bytes_in(page_size::SIZE_2M);
constexpr usize NATURAL_ALIGNMENT = 2 * sizeof(void *);

constinit bool m_heap_ready = false;
constinit u8  *m_break = nullptr;

u8 *claim_frame()
{
    return pmm::alloc(page_size::SIZE_2M)
        .map([](paddr_t frame) -> u8 * {
            void *window = vmm::phy_to_vrt(frame);

            if (window == nullptr)
                pmm::free(page_size::SIZE_2M, frame);

            return window as(u8 *);
        })
        .unwrap_or(nullptr);
}

void *morecore(ptrdiff_t increment)
{
    if (increment == 0)
        return m_break;

    if (increment < 0 || increment as(usize) > FRAME_BYTES)
        return (~uptr{0})as(void *);

    u8 *frame = claim_frame();

    if (frame == nullptr)
        return (~uptr{0})as(void *);

    m_break = frame + FRAME_BYTES;

    return frame;
}

[[noreturn]] void corrupted()
{
    KPANIC("kernel heap detected a corrupted chunk");
}

}  // namespace

#define DLMALLOC_EXPORT      static
#define USE_DL_PREFIX        1
#define HAVE_MORECORE        1
#define MORECORE             morecore
#define MORECORE_CONTIGUOUS  0
#define MORECORE_CANNOT_TRIM 1
#define HAVE_MMAP            0
#define HAVE_MREMAP          0
#define DEFAULT_GRANULARITY  ((size_t)2U * 1024U * 1024U)
#define USE_LOCKS            0
#define NO_MALLOC_STATS      1
#define MALLOC_FAILURE_ACTION
#define ABORT              corrupted()
#define malloc_getpagesize ((size_t)4096U)
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

    dlfree(ptr);
}

kernel::init::init_result component::init_heap()
{
    m_heap_ready = true;

    return kernel::core::Ok();
}

}  // namespace kernel::mm::kheap
