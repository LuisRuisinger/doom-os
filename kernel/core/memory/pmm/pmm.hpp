#ifndef DOOM_OS_KERNEL_CORE_MEMORY_PMM_HPP_
#define DOOM_OS_KERNEL_CORE_MEMORY_PMM_HPP_

// =================================================================================================
// Config files
// =================================================================================================

#include "config/layout.h"

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/boot_info.hpp"
#include "kernel/core/component.hpp"
#include "kernel/core/types.hpp"

namespace kernel::core::memory::pmm {

using kernel::core::paddr_t;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

// =================================================================================================
// Constants
//
// The allocator deals in frames, not bytes. A frame is the smallest unit the MMU can map, and
// the larger sizes are the other two the hardware understands - expressed as frame counts,
// because to this layer they are ordinary allocations that happen to be large and aligned.
// =================================================================================================

inline constexpr u64 FRAME_SIZE = 4096;
inline constexpr u64 FRAME_SHIFT = 12;

inline constexpr usize FRAMES_PER_2M = 512;
inline constexpr usize FRAMES_PER_1G = FRAMES_PER_2M * 512;

inline constexpr u64   MAX_PHYSICAL_MEMORY = DOOM_OS_MAX_PHYSICAL_MEMORY;
inline constexpr usize MAX_FRAMES = MAX_PHYSICAL_MEMORY / FRAME_SIZE;

inline constexpr paddr_t INVALID_PHYSICAL_ADDRESS = ~paddr_t{0};

static_assert(u64{1} << FRAME_SHIFT == FRAME_SIZE);
static_assert(MAX_PHYSICAL_MEMORY % (FRAMES_PER_1G * FRAME_SIZE) == 0,
              "the physical ceiling must be a whole number of 1 GiB blocks, so that no block at "
              "any level is partially outside the managed range");

// =================================================================================================
// Page sizes
//
// Named rather than inferred from a frame count, so a caller that happens to want 512 frames
// aligned to 512 gets what it asked for instead of whatever the arithmetic looked like.
// =================================================================================================

enum class page_size : u8 {
    SMALL_4K,
    LARGE_2M,
    HUGE_1G,
};

[[nodiscard]] inline constexpr usize frames_in(page_size size)
{
    switch (size) {
        case page_size::SMALL_4K:
            return 1;
        case page_size::LARGE_2M:
            return FRAMES_PER_2M;
        case page_size::HUGE_1G:
            return FRAMES_PER_1G;
    }

    return 0;
}

[[nodiscard]] inline constexpr u64 bytes_in(page_size size)
{
    return frames_in(size) * FRAME_SIZE;
}

// =================================================================================================
// PMM API
//
// alloc_pages hands back `count` pages of one size, each naturally aligned, and says nothing
// about where they are relative to one another. That is what makes it cheap: every page comes
// off an index lookup and nothing is ever scanned. It is all or nothing - either `out` is
// filled with `count` pages or nothing is allocated - so a caller never unwinds a partial
// result. `out` must have room for `count` entries.
//
// Misuse of the free functions - an unaligned or out of range address, a double free - is a
// bug in the caller rather than a condition to report, and panics.
// =================================================================================================

[[nodiscard]] bool alloc_pages(page_size size, usize count, paddr_t *out);
void free_pages(page_size size, usize count, const paddr_t *pages);

[[nodiscard]] paddr_t alloc_page(page_size size = page_size::SMALL_4K);
void free_page(page_size size, paddr_t base);

// =================================================================================================
// Component
// =================================================================================================

struct component : kernel::core::component<component, kernel::core::no_resource,
                                           kernel::boot::boot_info::component> {
    static constexpr auto *name = "PMM";

    static kernel::core::init_result init_allocator();

    template <typename View>
    static kernel::core::init_result init(View)
    {
        return init_allocator();
    }
};

}  // namespace kernel::core::memory::pmm

#endif  // DOOM_OS_KERNEL_CORE_MEMORY_PMM_HPP_
