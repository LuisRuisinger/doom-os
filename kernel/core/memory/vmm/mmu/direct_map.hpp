#ifndef DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_DIRECT_MAP_HPP_
#define DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_DIRECT_MAP_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/memory/vmm/mmu/pagetable.hpp"

namespace kernel::core::memory::vmm::mmu {

// =================================================================================================
// Direct map
//
// The window every physical address is reached through. It sits below both the page-table walker
// and the address-space layer: the walker needs it to touch a table it only knows by physical
// address, and it cannot ask the layer above for that without the two including each other.
// =================================================================================================

[[nodiscard]] bool direct_map_ready();
void mark_direct_map_ready();

[[nodiscard]] bool direct_map_contains(vaddr_t virtual_address);
[[nodiscard]] void *direct_map(paddr_t physical_address);
[[nodiscard]] paddr_t direct_map_physical(vaddr_t virtual_address);

// The alias through which a physical address can be touched *right now*.
//
// Once the VMM has switched CR3 that is the direct map. Before it, the early boot tables map the
// first DOOM_OS_EARLY_MAP_SIZE of physical memory at KERNEL_BASE, and the bootstrap tables are
// static objects inside the kernel image - so they are reachable that way while being built.
// Only walking may use this; allocation still waits for the direct map, because a frame from the
// PMM is not guaranteed to fall inside the early window.
[[nodiscard]] void *physical_window(paddr_t physical_address);

}  // namespace kernel::core::memory::vmm::mmu

#endif  // DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_DIRECT_MAP_HPP_
