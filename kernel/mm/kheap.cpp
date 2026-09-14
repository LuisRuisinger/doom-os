// =================================================================================================
// Cpp stdlib files
// =================================================================================================

#include <stddef.h>

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/mm/kheap.hpp"

#include "kernel/core/result.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/mm/page.hpp"
#include "kernel/mm/pmm.hpp"
#include "kernel/mm/vmm.hpp"

// =================================================================================================
// Third-party dlmalloc entry points (prefixed via USE_DL_PREFIX)
// =================================================================================================

extern "C" {
void *dlmalloc(size_t bytes);
void *dlmemalign(size_t alignment, size_t bytes);
void  dlfree(void *mem);
}

namespace {

using kernel::core::paddr_t;
using kernel::core::u8;
using kernel::core::uptr;
using kernel::core::usize;

namespace pmm = kernel::mm::pmm;
namespace vmm = kernel::mm::vmm;

constexpr usize FRAME_BYTES = 2 * 1024 * 1024;

static_assert(FRAME_BYTES == kernel::mm::PAGE_SIZE_2M,
              "the heap grows one MMU page at a time, so its unit is that page");

constexpr usize NATURAL_ALIGNMENT = 2 * sizeof(void *);

constinit bool m_heap_ready = false;
constinit u8  *m_break = nullptr;

void *morecore_failure()
{
    return reinterpret_cast<void *>(~uptr{0});
}

u8 *claim_frame()
{
    // The frame is only ours once it has a window: without one there is no way to reach it, so
    // the failed lookup hands it straight back rather than leaking it for the life of the kernel.
    return pmm::alloc_page(pmm::page_size::SIZE_2M)
        .map([](paddr_t frame) -> u8 * {
            void *window = vmm::phy_to_vrt(frame);

            if (window == nullptr) {
                pmm::free_page(pmm::page_size::SIZE_2M, frame);
                return nullptr;
            }

            return static_cast<u8 *>(window);
        })
        .unwrap_or(static_cast<u8 *>(nullptr));
}

}  // namespace

// =================================================================================================
// dlmalloc hooks
// =================================================================================================

extern "C" void *doom_os_kheap_morecore(ptrdiff_t increment)
{
    if (increment == 0)
        return m_break;

    if (increment < 0 || static_cast<usize>(increment) > FRAME_BYTES)
        return morecore_failure();

    u8 *frame = claim_frame();

    if (frame == nullptr)
        return morecore_failure();

    m_break = frame + FRAME_BYTES;

    return frame;
}

extern "C" void doom_os_kheap_abort()
{
    KPANIC("kernel heap detected a corrupted chunk");
}

namespace kernel::mm::kheap {

// =================================================================================================
// Allocation
// =================================================================================================

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

// =================================================================================================
// Init
// =================================================================================================

kernel::init::init_result component::init_heap()
{
    m_heap_ready = true;
    return kernel::core::Ok();
}

}  // namespace kernel::mm::kheap