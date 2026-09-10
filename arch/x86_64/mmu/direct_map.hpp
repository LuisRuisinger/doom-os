#ifndef DOOM_OS_ARCH_X86_64_MMU_DIRECT_MAP_HPP_
#define DOOM_OS_ARCH_X86_64_MMU_DIRECT_MAP_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "arch/x86_64/mmu/pagetable.hpp"

namespace kernel::arch::x86_64::mmu {

namespace detail {

// Which alias is live. A one-way flag rather than something derived, because the answer changes
// exactly once - at the CR3 switch - and the alternatives are all worse: reading CR3 back costs
// a register read on every table access, and neither alias is valid in both phases.
inline bool g_direct_map_ready = false;

}  // namespace detail

// =================================================================================================
// Direct map
//
// The window physical addresses are reached through. It sits below both the page-table walker and
// the address-space layer: the walker needs it to touch a table it only knows by physical address,
// and it cannot ask the layer above for that without the two including each other.
// =================================================================================================

[[nodiscard]] inline bool direct_map_ready()
{
    return detail::g_direct_map_ready;
}

inline void mark_direct_map_ready()
{
    detail::g_direct_map_ready = true;
}

// The alias through which a physical address can be touched *right now*, or nullptr if it has
// none - past the window there is no mapping, and returning base + physical anyway yields a
// pointer that looks fine and faults on use.
//
// Once the VMM has switched CR3 that is the direct map. Before it, the early boot tables map the
// first EARLY_MAP_SIZE of physical memory at KERNEL_BASE, and the bootstrap tables are static
// objects inside the kernel image - so they are reachable that way while being built. Only
// walking may rely on the early phase; allocation still waits for the direct map, because a frame
// from the PMM is not guaranteed to fall inside the early window.
[[nodiscard]] inline void *phy_to_vrt(paddr_t physical_address)
{
    if (direct_map_ready())
        return physical_address < DIRECT_MAP_SIZE
                   ? reinterpret_cast<void *>(DIRECT_MAP_BASE + physical_address)
                   : nullptr;

    return physical_address < EARLY_MAP_SIZE
               ? reinterpret_cast<void *>(KERNEL_BASE + physical_address)
               : nullptr;
}

}  // namespace kernel::arch::x86_64::mmu

#endif  // DOOM_OS_ARCH_X86_64_MMU_DIRECT_MAP_HPP_
