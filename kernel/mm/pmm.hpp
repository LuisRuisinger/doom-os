#ifndef DOOM_OS_KERNEL_MM_PMM_HPP_
#define DOOM_OS_KERNEL_MM_PMM_HPP_

// =================================================================================================
// Config files
// =================================================================================================

#include "config/layout.h"

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/boot_info.hpp"
#include "kernel/init/component.hpp"
#include "kernel/mm/page.hpp"
#include "kernel/core/types.hpp"

namespace kernel::mm::pmm {

using kernel::core::paddr_t;
using kernel::core::u64;
using kernel::core::usize;
using kernel::mm::bytes_in;
using kernel::mm::frames_in;
using kernel::mm::FRAME_SHIFT;
using kernel::mm::FRAME_SIZE;
using kernel::mm::FRAMES_PER_1G;
using kernel::mm::FRAMES_PER_2M;
using kernel::mm::is_page_size;
using kernel::mm::is_sw_allocatable;
using kernel::mm::page;
using kernel::mm::page_1g;
using kernel::mm::page_2m;
using kernel::mm::page_4k;
using kernel::mm::page_size;
using kernel::mm::page_size_of;
using kernel::mm::PAGE_SIZE_1G;
using kernel::mm::PAGE_SIZE_2M;
using kernel::mm::PAGE_SIZE_4K;

// =================================================================================================
// Constants
//
// The allocator deals in frames. The three sizes it hands out are the three the MMU can map,
// expressed as frame counts, because to this layer they are ordinary allocations that happen to
// be large and aligned.
// =================================================================================================

inline constexpr u64   MAX_PHYSICAL_MEMORY = DOOM_OS_MAX_PHYSICAL_MEMORY;
inline constexpr usize MAX_FRAMES = MAX_PHYSICAL_MEMORY / FRAME_SIZE;

inline constexpr paddr_t INVALID_PHYSICAL_ADDRESS = ~paddr_t{0};

static_assert(u64{1} << FRAME_SHIFT == FRAME_SIZE);
static_assert(MAX_PHYSICAL_MEMORY % PAGE_SIZE_1G == 0,
              "the physical ceiling must be a whole number of 1 GiB blocks, so that no block is "
              "partially outside the managed range");

// =================================================================================================
// PMM API
//
// alloc_pages hands back `count` pages of one size, each naturally aligned, and says nothing
// about where they are relative to one another. It is all or nothing, so a caller never unwinds
// a partial result, and `out` must have room for `count` entries.
//
// Misuse of the release functions - an unaligned or out of range address, a double free - is a
// bug in the caller rather than a condition to report, and panics.
// =================================================================================================

[[nodiscard]] bool alloc_pages(page_size size, usize count, paddr_t *out);
void free_pages(page_size size, usize count, const paddr_t *pages);

[[nodiscard]] paddr_t alloc_page(page_size size = page_size::SIZE_4K);
void free_page(page_size size, paddr_t base);

// =================================================================================================
// Typed spans
//
// A page named by its byte count, so a call site says span_2m and is wrong at compile time
// rather than at run time.
// =================================================================================================

template <u64 Bytes>
struct span {
    static_assert(is_page_size(Bytes), "a span has to be a size the MMU can map");

    static constexpr page_size SIZE = page_size_of(Bytes);
    static constexpr u64       BYTES = Bytes;
    static constexpr usize     FRAMES = frames_in(SIZE);

    [[nodiscard]] static paddr_t alloc()
    {
        return alloc_page(SIZE);
    }

    static void free(paddr_t base)
    {
        free_page(SIZE, base);
    }
};

using span_4k = span<FRAME_SIZE>;
using span_2m = span<PAGE_SIZE_2M>;
using span_1g = span<PAGE_SIZE_1G>;

// =================================================================================================
// Component
// =================================================================================================

struct component : kernel::init::component<component, kernel::init::no_resource,
                                           kernel::boot::boot_info::component> {
    static constexpr auto *name = "PMM";

    static kernel::init::init_result init_allocator();

    template <typename View>
    static kernel::init::init_result init(View)
    {
        return init_allocator();
    }
};

}  // namespace kernel::mm::pmm

#endif  // DOOM_OS_KERNEL_MM_PMM_HPP_
