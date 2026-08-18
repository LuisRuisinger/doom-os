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

        // A subtree is either kernel or user. Quietly ORing USER in would leave every kernel leaf
        // already below this entry reachable from ring 3, and nothing ever clears the bit again -
        // so a mismatch is refused rather than resolved in the permissive direction.
        if (user != ((entry & ENTRY_USER) != 0))
            return false;

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

[[nodiscard]] bool split_large_entry(u64 &entry, page_size large_size)
{
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

    // Replacing a mapping of the same size is what map() means. A different size is refused: the
    // other 511 pages of a large page belong to whoever established it, and tearing them down
    // because this address was re-mapped is not a decision to make silently.
    if (present(entry) && large(entry) != (size != page_size::SIZE_4K))
        return false;

    entry = leaf_entry(physical, size, flags);
    return true;
}

// =================================================================================================
// Leaf lookup
//
// map() has to create tables on the way down, so it walks on its own. Everything else only wants
// the leaf that already covers an address, at whatever size it happens to be - one walk rather
// than the three near-identical ones this used to carry.
// =================================================================================================

struct leaf_location {
    u64      *entry{};
    page_size size{};
};

[[nodiscard]] leaf_location find_leaf(paddr_t root, vaddr_t virtual_address)
{
    page_table *pml4 = table_from_physical(root);

    if (pml4 == nullptr)
        return {};

    u64 &pml4_entry = (*pml4)[pml4_index(virtual_address)];

    if (!present(pml4_entry) || large(pml4_entry))
        return {};

    page_table *pdpt = table_from_physical(entry_address(pml4_entry));

    if (pdpt == nullptr)
        return {};

    u64 &pdpt_entry = (*pdpt)[pdpt_index(virtual_address)];

    if (!present(pdpt_entry))
        return {};

    if (large(pdpt_entry))
        return {.entry = &pdpt_entry, .size = page_size::SIZE_1G};

    page_table *pd = table_from_physical(entry_address(pdpt_entry));

    if (pd == nullptr)
        return {};

    u64 &pd_entry = (*pd)[pd_index(virtual_address)];

    if (!present(pd_entry))
        return {};

    if (large(pd_entry))
        return {.entry = &pd_entry, .size = page_size::SIZE_2M};

    page_table *pt = table_from_physical(entry_address(pd_entry));

    if (pt == nullptr)
        return {};

    u64 &pt_entry = (*pt)[pt_index(virtual_address)];

    if (!present(pt_entry))
        return {};

    return {.entry = &pt_entry, .size = page_size::SIZE_4K};
}

[[nodiscard]] u64 page_offset(vaddr_t address, page_size size)
{
    return address & (bytes_in(size) - 1);
}

// =================================================================================================
// Collecting empty tables
// =================================================================================================

paddr_t g_reserved_first{};
paddr_t g_reserved_last{};

[[nodiscard]] bool table_is_reclaimable(paddr_t physical)
{
    return physical < g_reserved_first || physical >= g_reserved_last;
}

[[nodiscard]] bool table_is_empty(const page_table &table)
{
    for (usize i = 0; i < ENTRIES_PER_TABLE; ++i)
        if (table[i] != 0)
            return false;

    return true;
}

// Called after a leaf is cleared. Without this an address space only ever grows: a table that
// mapped one page keeps its frame forever once that page goes away.
void reclaim_empty_tables(paddr_t root, vaddr_t virtual_address)
{
    page_table *pml4 = table_from_physical(root);

    if (pml4 == nullptr)
        return;

    // The entries pointing at the PDPT, the PD and the PT, in that order.
    u64  *chain[3]{};
    usize depth = 0;

    u64 &pml4_entry = (*pml4)[pml4_index(virtual_address)];

    if (!present(pml4_entry) || large(pml4_entry))
        return;

    chain[depth++] = &pml4_entry;

    page_table *pdpt = table_from_physical(entry_address(pml4_entry));

    if (pdpt != nullptr) {
        u64 &pdpt_entry = (*pdpt)[pdpt_index(virtual_address)];

        if (present(pdpt_entry) && !large(pdpt_entry)) {
            chain[depth++] = &pdpt_entry;

            page_table *pd = table_from_physical(entry_address(pdpt_entry));

            if (pd != nullptr) {
                u64 &pd_entry = (*pd)[pd_index(virtual_address)];

                if (present(pd_entry) && !large(pd_entry))
                    chain[depth++] = &pd_entry;
            }
        }
    }

    // Deepest first: a table can only be collected once the one below it is gone, so the first
    // level that is still holding something stops the unwind.
    while (depth > 0) {
        u64          &entry = *chain[--depth];
        const paddr_t physical = entry_address(entry);
        page_table   *table = table_from_physical(physical);

        if (table == nullptr || !table_is_empty(*table) || !table_is_reclaimable(physical))
            return;

        entry = 0;
        kernel::core::memory::pmm::span_4k::free(physical);
    }
}

}  // namespace

// =================================================================================================
// Reserved tables
// =================================================================================================

void set_reserved_table_range(paddr_t first, paddr_t last)
{
    g_reserved_first = first;
    g_reserved_last = last;
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
// Address space
// =================================================================================================

bool address_space::map(vaddr_t virtual_address, paddr_t physical_address, page_size size,
                        page_flags flags)
{
    const u64 page_bytes = bytes_in(size);

    // A 1 GiB leaf on a CPU without them is not a slow mapping, it is a reserved-bit fault on
    // first touch. The size is part of the public API, so the check belongs here rather than in
    // whoever happened to pick it.
    if (size == page_size::SIZE_1G && !features().gib_pages)
        return false;

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
        !split_large_entry(pdpt_entry, page_size::SIZE_1G))
        return false;

    if (!ensure_child_table(*pdpt, pdpt_index(virtual_address), flags.user, pd))
        return false;

    u64 &pd_entry = (*pd)[pd_index(virtual_address)];

    if (size == page_size::SIZE_2M)
        return set_leaf(*pd, pd_index(virtual_address), physical_address, size, flags);

    if (present(pd_entry) && large(pd_entry) && !split_large_entry(pd_entry, page_size::SIZE_2M))
        return false;

    if (!ensure_child_table(*pd, pd_index(virtual_address), flags.user, pt))
        return false;

    return set_leaf(*pt, pt_index(virtual_address), physical_address, size, flags);
}

bool address_space::protect(vaddr_t virtual_address, page_size size, page_flags flags)
{
    if (!valid() || !is_canonical(virtual_address) ||
        !is_aligned<vaddr_t>(virtual_address, bytes_in(size)))
        return false;

    const leaf_location leaf = find_leaf(root_m, virtual_address);

    if (leaf.entry == nullptr || leaf.size != size)
        return false;

    *leaf.entry = leaf_entry(entry_address(*leaf.entry), size, flags);
    return true;
}

bool address_space::unmap(vaddr_t virtual_address, page_size size)
{
    if (!valid() || !is_canonical(virtual_address) ||
        !is_aligned<vaddr_t>(virtual_address, bytes_in(size)))
        return false;

    // Unmapping something smaller than the leaf that covers it means splitting that leaf first,
    // so the neighbours it also covered survive. map() has always split on the way down; not
    // doing the same here made the two directions disagree about what a 4 KiB address means.
    for (;;) {
        const leaf_location leaf = find_leaf(root_m, virtual_address);

        if (leaf.entry == nullptr)
            return false;

        if (leaf.size == size) {
            *leaf.entry = 0;
            reclaim_empty_tables(root_m, virtual_address);
            return true;
        }

        if (bytes_in(leaf.size) < bytes_in(size))
            return false;

        if (!split_large_entry(*leaf.entry, leaf.size))
            return false;
    }
}

mapping address_space::translate(vaddr_t virtual_address) const
{
    if (!valid() || !is_canonical(virtual_address))
        return {};

    const leaf_location leaf = find_leaf(root_m, virtual_address);

    if (leaf.entry == nullptr)
        return {};

    return mapping{
        .present = true,
        .frame = entry_address(*leaf.entry),
        .offset = page_offset(virtual_address, leaf.size),
        .size = leaf.size,
        .flags = flags_from_entry(*leaf.entry),
    };
}

}  // namespace kernel::core::memory::vmm::mmu
