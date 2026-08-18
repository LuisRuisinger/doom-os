// =================================================================================================
// Config files
// =================================================================================================

#include "config/layout.h"

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/memory/pmm/pmm.hpp"
#include "kernel/core/memory/vmm/mmu/direct_map.hpp"

namespace kernel::core::memory::vmm::mmu {

namespace {

inline constexpr u64 EARLY_MAP_SIZE = static_cast<u64>(DOOM_OS_EARLY_MAP_SIZE);

bool g_ready{};

}  // namespace

// =================================================================================================
// Direct map
// =================================================================================================

bool direct_map_ready()
{
    return g_ready;
}

void mark_direct_map_ready()
{
    g_ready = true;
}

bool direct_map_contains(vaddr_t virtual_address)
{
    return virtual_address >= DIRECT_MAP_BASE &&
           virtual_address - DIRECT_MAP_BASE < DIRECT_MAP_SIZE;
}

void *direct_map(paddr_t physical_address)
{
    // Past the window there is no alias to hand back. Returning DIRECT_MAP_BASE + physical
    // anyway lands in the next PML4 slot, which is a pointer that looks fine and faults on use.
    if (physical_address >= DIRECT_MAP_SIZE)
        return nullptr;

    return reinterpret_cast<void *>(DIRECT_MAP_BASE + physical_address);
}

paddr_t direct_map_physical(vaddr_t virtual_address)
{
    return direct_map_contains(virtual_address)
               ? static_cast<paddr_t>(virtual_address - DIRECT_MAP_BASE)
               : kernel::core::memory::pmm::INVALID_PHYSICAL_ADDRESS;
}

void *physical_window(paddr_t physical_address)
{
    if (g_ready)
        return direct_map(physical_address);

    // Reached only while the bootstrap tables are being built, and everything touched then is
    // inside the kernel image. A physical address past the early window has no alias at all, so
    // returning one would hand back a pointer that faults on first use.
    if (physical_address >= EARLY_MAP_SIZE)
        return nullptr;

    return reinterpret_cast<void *>(KERNEL_BASE + physical_address);
}

}  // namespace kernel::core::memory::vmm::mmu
