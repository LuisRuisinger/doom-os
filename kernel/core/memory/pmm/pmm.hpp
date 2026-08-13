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
// Statistics
// =================================================================================================

struct stats {
    bool  initialized{};
    usize managed_frames{};
    usize free_frames{};
    usize allocated_frames{};
    usize free_2m_blocks{};
    usize free_1g_blocks{};
};

// =================================================================================================
// PMM API
//
// alloc_frames returns a physical address aligned to alignment_frames * FRAME_SIZE, or
// INVALID_PHYSICAL_ADDRESS if the request cannot be met. alignment_frames must be a power of
// two. The MMU frame sizes are ordinary requests here:
//
//     alloc_frames(FRAMES_PER_2M, FRAMES_PER_2M)
//
// free_frames is given the count the caller asked for. Misuse - an unaligned or out of range
// address, a wrong count, a double free - is a bug in the caller rather than a condition to
// report, and panics.
// =================================================================================================

bool initialized();
stats current_stats();

paddr_t alloc_frames(usize count, usize alignment_frames = 1);
paddr_t alloc_frame();

void free_frames(paddr_t base, usize count);
void free_frame(paddr_t base);

bool is_free(paddr_t address);
bool contains(paddr_t address);

// Recomputes every derived level from the frame bitmap and compares. Everything above the
// frame bitmap is a summary of it, so this catches any bookkeeping mistake in split or
// coalesce without needing to know what the caller expected.
bool verify_invariants();

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
