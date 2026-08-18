// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/memory/vmm/mmu/pagetable.hpp"

#include "kernel/core/bits.hpp"
#include "kernel/core/memory/pmm/pmm.hpp"
#include "kernel/core/memory/vmm/mmu/direct_map.hpp"
#include "kernel/core/memory/vmm/mmu/entry.hpp"

namespace kernel::core::memory::vmm::mmu {

namespace {

using kernel::core::utils::is_aligned;

[[nodiscard]] bool is_canonical(vaddr_t address)
{
    const u64 sign = (address >> 47) & 1;
    const u64 upper = address >> 48;

    return sign == 0 ? upper == 0 : upper == 0xFFFF;
}

[[nodiscard]] u64 split_child_step(page_size large_size)
{
    return large_size == page_size::SIZE_1G ? PAGE_SIZE_2M : PAGE_SIZE_4K;
}

[[nodiscard]] page_table *table_from_physical(paddr_t physical)
{
    return static_cast<page_table *>(physical_window(physical));
}

[[nodiscard]] paddr_t alloc_table()
{
    // Allocation waits for the direct map even though a walk does not: a frame from the PMM can
    // sit anywhere in physical memory, and only the direct map reaches all of it.
    if (!direct_map_ready())
        return kernel::core::memory::pmm::INVALID_PHYSICAL_ADDRESS;

    const paddr_t physical = kernel::core::memory::pmm::span_4k::alloc();

    if (physical == kernel::core::memory::pmm::INVALID_PHYSICAL_ADDRESS)
        return physical;

    table_from_physical(physical)->clear();
    return physical;
}

[[nodiscard]] bool ensure_child_table(page_table &parent, usize index, bool user,
                                      page_table *&child)
{
    u64 &entry = parent[index];

    if (present(entry)) {
        if (large(entry))
            return false;

        if (user)
            entry |= ENTRY_USER;

        entry |= ENTRY_WRITE;
        child = table_from_physical(entry_address(entry));
        return child != nullptr;
    }

    const paddr_t physical = alloc_table();

    if (physical == kernel::core::memory::pmm::INVALID_PHYSICAL_ADDRESS)
        return false;

    entry = table_entry(physical, user);
    child = table_from_physical(physical);
    return child != nullptr;
}

[[nodiscard]] bool split_large_entry(page_table &parent, usize index, page_size large_size)
{
    u64 &entry = parent[index];

    if (!present(entry) || !large(entry))
        return false;

    const paddr_t child_physical = alloc_table();

    if (child_physical == kernel::core::memory::pmm::INVALID_PHYSICAL_ADDRESS)
        return false;

    page_table   *child = table_from_physical(child_physical);
    const u64     step = split_child_step(large_size);
    const u64     child_large = large_size == page_size::SIZE_1G ? ENTRY_LARGE : 0;
    const u64     child_flags = entry & ENTRY_INHERITED_FLAGS;
    const bool    user = (entry & ENTRY_USER) != 0;
    const paddr_t physical = entry_address(entry);

    for (usize i = 0; i < ENTRIES_PER_TABLE; ++i)
        (*child)[i] = ((physical + i * step) & ENTRY_ADDRESS_MASK) | child_flags | child_large;

    entry = table_entry(child_physical, user);

    return true;
}

[[nodiscard]] bool set_leaf(page_table &table, usize index, paddr_t physical, page_size size,
                            page_flags flags)
{
    u64 &entry = table[index];

    if (present(entry)) {
        const bool existing_large = large(entry);
        const bool requested_large = size != page_size::SIZE_4K;

        if (existing_large != requested_large || entry_address(entry) != physical)
            return false;
    }

    entry = leaf_entry(physical, size, flags);
    return true;
}

[[nodiscard]] bool leaf_matches_size(u64 entry, page_size size)
{
    return size == page_size::SIZE_4K ? !large(entry) : large(entry);
}

[[nodiscard]] u64 page_offset(vaddr_t address, page_size size)
{
    return address & (bytes_in(size) - 1);
}

}  // namespace

// =================================================================================================
// Page table
// =================================================================================================

void page_table::clear()
{
    for (usize i = 0; i < ENTRIES_PER_TABLE; ++i)
        entries_m[i] = 0;
}

// =================================================================================================
// Address space
// =================================================================================================

bool address_space::map(vaddr_t virtual_address, paddr_t physical_address, page_size size,
                        page_flags flags)
{
    const u64 page_bytes = bytes_in(size);

    if (!valid() || !is_canonical(virtual_address) ||
        !is_aligned<vaddr_t>(virtual_address, page_bytes) ||
        !is_aligned<paddr_t>(physical_address, page_bytes))
        return false;

    page_table *pml4 = table_from_physical(root_m);
    page_table *pdpt = nullptr;
    page_table *pd = nullptr;
    page_table *pt = nullptr;

    if (pml4 == nullptr)
        return false;

    if (!ensure_child_table(*pml4, pml4_index(virtual_address), flags.user, pdpt))
        return false;

    u64 &pdpt_entry = (*pdpt)[pdpt_index(virtual_address)];

    if (size == page_size::SIZE_1G)
        return set_leaf(*pdpt, pdpt_index(virtual_address), physical_address, size, flags);

    if (present(pdpt_entry) && large(pdpt_entry) &&
        !split_large_entry(*pdpt, pdpt_index(virtual_address), page_size::SIZE_1G))
        return false;

    if (!ensure_child_table(*pdpt, pdpt_index(virtual_address), flags.user, pd))
        return false;

    u64 &pd_entry = (*pd)[pd_index(virtual_address)];

    if (size == page_size::SIZE_2M)
        return set_leaf(*pd, pd_index(virtual_address), physical_address, size, flags);

    if (present(pd_entry) && large(pd_entry) &&
        !split_large_entry(*pd, pd_index(virtual_address), page_size::SIZE_2M))
        return false;

    if (!ensure_child_table(*pd, pd_index(virtual_address), flags.user, pt))
        return false;

    return set_leaf(*pt, pt_index(virtual_address), physical_address, size, flags);
}

bool address_space::unmap(vaddr_t virtual_address, page_size size)
{
    const u64 page_bytes = bytes_in(size);

    if (!valid() || !is_canonical(virtual_address) ||
        !is_aligned<vaddr_t>(virtual_address, page_bytes))
        return false;

    page_table *pml4 = table_from_physical(root_m);

    if (pml4 == nullptr)
        return false;

    const u64 pml4_entry = (*pml4)[pml4_index(virtual_address)];

    if (!present(pml4_entry) || large(pml4_entry))
        return false;

    page_table *pdpt = table_from_physical(entry_address(pml4_entry));
    u64        &pdpt_entry = (*pdpt)[pdpt_index(virtual_address)];

    if (!present(pdpt_entry))
        return false;

    if (size == page_size::SIZE_1G) {
        if (!leaf_matches_size(pdpt_entry, size))
            return false;

        pdpt_entry = 0;
        return true;
    }

    if (large(pdpt_entry))
        return false;

    page_table *pd = table_from_physical(entry_address(pdpt_entry));
    u64        &pd_entry = (*pd)[pd_index(virtual_address)];

    if (!present(pd_entry))
        return false;

    if (size == page_size::SIZE_2M) {
        if (!leaf_matches_size(pd_entry, size))
            return false;

        pd_entry = 0;
        return true;
    }

    if (large(pd_entry))
        return false;

    page_table *pt = table_from_physical(entry_address(pd_entry));
    u64        &pt_entry = (*pt)[pt_index(virtual_address)];

    if (!present(pt_entry) || large(pt_entry))
        return false;

    pt_entry = 0;
    return true;
}

mapping address_space::translate(vaddr_t virtual_address) const
{
    if (!valid() || !is_canonical(virtual_address))
        return {};

    const page_table *pml4 = table_from_physical(root_m);

    if (pml4 == nullptr)
        return {};

    const u64 pml4_entry = (*pml4)[pml4_index(virtual_address)];

    if (!present(pml4_entry) || large(pml4_entry))
        return {};

    const page_table *pdpt = table_from_physical(entry_address(pml4_entry));
    const u64         pdpt_entry = (*pdpt)[pdpt_index(virtual_address)];

    if (!present(pdpt_entry))
        return {};

    if (large(pdpt_entry)) {
        const page_size size = page_size::SIZE_1G;

        return mapping{
            .present = true,
            .physical = entry_address(pdpt_entry) + page_offset(virtual_address, size),
            .size = size,
            .flags = flags_from_entry(pdpt_entry),
        };
    }

    const page_table *pd = table_from_physical(entry_address(pdpt_entry));
    const u64         pd_entry = (*pd)[pd_index(virtual_address)];

    if (!present(pd_entry))
        return {};

    if (large(pd_entry)) {
        const page_size size = page_size::SIZE_2M;

        return mapping{
            .present = true,
            .physical = entry_address(pd_entry) + page_offset(virtual_address, size),
            .size = size,
            .flags = flags_from_entry(pd_entry),
        };
    }

    const page_table *pt = table_from_physical(entry_address(pd_entry));
    const u64         pt_entry = (*pt)[pt_index(virtual_address)];

    if (!present(pt_entry))
        return {};

    const page_size size = page_size::SIZE_4K;

    return mapping{
        .present = true,
        .physical = entry_address(pt_entry) + page_offset(virtual_address, size),
        .size = size,
        .flags = flags_from_entry(pt_entry),
    };
}

}  // namespace kernel::core::memory::vmm::mmu
