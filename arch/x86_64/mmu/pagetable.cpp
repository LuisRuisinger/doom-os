// =================================================================================================
// Kernel files
// =================================================================================================

#include "arch/x86_64/mmu/pagetable.hpp"

#include "arch/common/mmu/hw_radix_tree.hpp"
#include "arch/x86_64/mmu/environment.hpp"
#include "arch/x86_64/mmu/features.hpp"
#include "arch/x86_64/mmu/traits.hpp"
#include "kernel/core/bits.hpp"

namespace kernel::arch::x86_64::mmu {

namespace {

using kernel::core::utils::is_aligned;

paddr_t g_reserved_first{};
paddr_t g_reserved_last{};

[[nodiscard]] bool is_canonical(vaddr_t address)
{
    const u64 sign  = (address >> 47) & 1;
    const u64 upper = address >> 48;
    return sign == 0 ? upper == 0 : upper == 0xFFFF;
}

using page_tree = kernel::arch::common::mmu::hw_radix_tree<x86_64_paging_traits, default_paging_environment>;

}  // namespace

// =================================================================================================
// Environment hooks
// =================================================================================================

void set_reserved_table_range(paddr_t first, paddr_t last)
{
    g_reserved_first = first;
    g_reserved_last  = last;
}

bool default_paging_environment::is_reclaimable(paddr_t phys)
{
    return phys < g_reserved_first || phys >= g_reserved_last;
}

// =================================================================================================
// Page table
// =================================================================================================

void page_table::clear()
{
    for (usize i = 0; i < ENTRIES_PER_TABLE; ++i)
        entries_m[i] = 0;
}

// =================================================================================================
// Address space facade
// =================================================================================================

bool address_space::map(vaddr_t virtual_address, paddr_t physical_address, page_size size,
                        page_flags flags)
{
    if (size == page_size::SIZE_1G && !gib_pages_supported())
        return false;

    const u64 page_bytes = bytes_in(size);

    if (!valid() || !is_canonical(virtual_address) ||
        !is_aligned<vaddr_t>(virtual_address, page_bytes) ||
        !is_aligned<paddr_t>(physical_address, page_bytes))
        return false;

    return page_tree{root_m}.map(virtual_address, physical_address, size, flags);
}

bool address_space::protect(vaddr_t virtual_address, page_size size, page_flags flags)
{
    if (!valid() || !is_canonical(virtual_address) ||
        !is_aligned<vaddr_t>(virtual_address, bytes_in(size)))
        return false;

    return page_tree{root_m}.protect(virtual_address, size, flags);
}

bool address_space::unmap(vaddr_t virtual_address, page_size size)
{
    if (!valid() || !is_canonical(virtual_address) ||
        !is_aligned<vaddr_t>(virtual_address, bytes_in(size)))
        return false;

    return page_tree{root_m}.unmap(virtual_address, size);
}

mapping address_space::translate(vaddr_t virtual_address) const
{
    if (!valid() || !is_canonical(virtual_address))
        return {};

    return page_tree{root_m}.template translate<mapping>(virtual_address);
}

}  // namespace kernel::arch::x86_64::mmu