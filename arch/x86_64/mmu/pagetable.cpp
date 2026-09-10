// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/memory/vmm/mmu/pagetable.hpp"

#include "kernel/core/bits.hpp"
#include "kernel/core/memory/pmm/pmm.hpp"
#include "kernel/core/memory/vmm/mmu/direct_map.hpp"
#include "kernel/core/memory/vmm/mmu/entry.hpp"
#include "kernel/core/memory/vmm/mmu/features.hpp"

namespace kernel::core::memory::vmm::mmu {

namespace {

using kernel::core::utils::is_aligned;

[[nodiscard]] bool is_canonical(vaddr_t address)
{
    const u64 sign = (address >> 47) & 1;
    const u64 upper = address >> 48;

    return sign == 0 ? upper == 0 : upper == 0xFFFF;
}

[[nodiscard]] page_table *table_from_physical(paddr_t physical)
{
    return static_cast<page_table *>(physical_window(physical));
}

[[nodiscard]] paddr_t alloc_table()
{
    if (!direct_map_ready())
        return kernel::core::memory::pmm::INVALID_PHYSICAL_ADDRESS;

    const paddr_t physical = kernel::core::memory::pmm::span_4k::alloc();
    if (physical == kernel::core::memory::pmm::INVALID_PHYSICAL_ADDRESS)
        return physical;

    table_from_physical(physical)->clear();
    return physical;
}

enum class table_level : u8 {
    PML4,
    PDPT,
    PD,
    PT,
};

[[nodiscard]] constexpr bool has_child_level(table_level level)
{
    return level != table_level::PT;
}

[[nodiscard]] constexpr table_level child_level(table_level level)
{
    switch (level) {
        case table_level::PML4:
            return table_level::PDPT;
        case table_level::PDPT:
            return table_level::PD;
        case table_level::PD:
            return table_level::PT;
        case table_level::PT:
            return table_level::PT;
    }

    return table_level::PT;
}

[[nodiscard]] constexpr u64 span_at(table_level level)
{
    switch (level) {
        case table_level::PML4:
            return PML4_SLOT_SIZE;
        case table_level::PDPT:
            return PAGE_SIZE_1G;
        case table_level::PD:
            return PAGE_SIZE_2M;
        case table_level::PT:
            return PAGE_SIZE_4K;
    }

    return 0;
}

[[nodiscard]] constexpr page_size page_size_at(table_level level)
{
    switch (level) {
        case table_level::PDPT:
            return page_size::SIZE_1G;
        case table_level::PD:
            return page_size::SIZE_2M;
        case table_level::PT:
            return page_size::SIZE_4K;
        case table_level::PML4:
            return page_size::SIZE_1G;
    }

    return page_size::SIZE_1G;
}

[[nodiscard]] constexpr bool maps_page_size(table_level level, page_size size)
{
    return (level == table_level::PDPT && size == page_size::SIZE_1G) ||
           (level == table_level::PD && size == page_size::SIZE_2M) ||
           (level == table_level::PT && size == page_size::SIZE_4K);
}

[[nodiscard]] constexpr usize index_at(table_level level, vaddr_t address)
{
    switch (level) {
        case table_level::PML4:
            return pml4_index(address);
        case table_level::PDPT:
            return pdpt_index(address);
        case table_level::PD:
            return pd_index(address);
        case table_level::PT:
            return pt_index(address);
    }

    return 0;
}

class table_node {
    page_table  *table_m{};
    table_level  level_m{table_level::PML4};

public:
    constexpr table_node() = default;

    constexpr table_node(page_table *table, table_level level)
        : table_m(table),
          level_m(level)
    {
    }

    [[nodiscard]] static table_node from_physical(paddr_t physical, table_level level)
    {
        return table_node{table_from_physical(physical), level};
    }

    [[nodiscard]] bool valid() const
    {
        return table_m != nullptr;
    }

    [[nodiscard]] table_level level() const
    {
        return level_m;
    }

    [[nodiscard]] bool maps(page_size size) const
    {
        return maps_page_size(level_m, size);
    }

    [[nodiscard]] u64 &entry(vaddr_t address) const
    {
        return (*table_m)[index_at(level_m, address)];
    }

    [[nodiscard]] table_node child(u64 entry) const
    {
        return from_physical(entry_address(entry), child_level(level_m));
    }

    [[nodiscard]] bool ensure_child(vaddr_t address, bool user, table_node &out) const
    {
        if (!has_child_level(level_m))
            return false;

        u64 &slot = entry(address);

        if (present(slot)) {
            if (large(slot))
                return false;

            // A subtree is either kernel or user. Quietly ORing USER in would leave every kernel
            // leaf already below this entry reachable from ring 3, and nothing ever clears the bit
            // again - so a mismatch is refused rather than resolved in the permissive direction.
            if (user != ((slot & PAGE_FLAG_USER) != 0))
                return false;

            slot |= PAGE_FLAG_WRITABLE;
            out = child(slot);
            return out.valid();
        }

        const paddr_t physical = alloc_table();

        if (physical == kernel::core::memory::pmm::INVALID_PHYSICAL_ADDRESS)
            return false;

        slot = table_entry(physical, user);
        out = table_node::from_physical(physical, child_level(level_m));
        return out.valid();
    }

    [[nodiscard]] bool split_large_leaf(u64 &slot) const
    {
        if (!present(slot) || !large(slot) || level_m == table_level::PML4 ||
            level_m == table_level::PT)
            return false;

        const paddr_t child_physical = alloc_table();

        if (child_physical == kernel::core::memory::pmm::INVALID_PHYSICAL_ADDRESS)
            return false;

        table_node child_node = table_node::from_physical(child_physical, child_level(level_m));

        if (!child_node.valid())
            return false;

        const u64     child_step = span_at(child_node.level());
        const u64     child_large = child_node.level() == table_level::PD ? ENTRY_LARGE : 0;
        const u64     child_flags = slot & ENTRY_INHERITED_FLAGS;
        const bool    user = (slot & PAGE_FLAG_USER) != 0;
        const paddr_t physical = entry_address(slot);

        for (usize i = 0; i < ENTRIES_PER_TABLE; ++i)
            (*child_node.table_m)[i] =
                ((physical + i * child_step) & ENTRY_ADDRESS_MASK) | child_flags | child_large;

        slot = table_entry(child_physical, user);

        return true;
    }

    [[nodiscard]] bool set_leaf(vaddr_t address, paddr_t physical, page_size size,
                                page_flags flags) const
    {
        u64 &slot = entry(address);

        // Replacing a mapping of the same size is what map() means. A different size is refused:
        // the other 511 pages of a large page belong to whoever established it, and tearing them
        // down because this address was re-mapped is not a decision to make silently.
        if (present(slot) && large(slot) != (size != page_size::SIZE_4K))
            return false;

        slot = leaf_entry(physical, size, flags);
        return true;
    }
};

struct leaf_location {
    u64      *entry{};
    page_size size{};
};

struct table_path {
    u64  *entries[3]{};
    usize depth{};

    void push(u64 &entry)
    {
        entries[depth++] = &entry;
    }
};

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

// =================================================================================================
// Page-table tree
//
// The hardware page tables are a radix tree of 4 KiB table pages. PMM owns the frames, while this
// layer gives those frames tree semantics: create children, split leaves, find leaves and reclaim
// empty descendants.
// =================================================================================================

class page_table_tree {
    paddr_t root_m{};

    [[nodiscard]] table_node root() const
    {
        return table_node::from_physical(root_m, table_level::PML4);
    }

    [[nodiscard]] table_path path_to(vaddr_t virtual_address) const
    {
        table_path path{};
        table_node node = root();

        while (node.valid() && has_child_level(node.level()) && path.depth < 3) {
            u64 &slot = node.entry(virtual_address);

            if (!present(slot) || large(slot))
                break;

            path.push(slot);
            node = node.child(slot);
        }

        return path;
    }

public:
    explicit constexpr page_table_tree(paddr_t root)
        : root_m(root)
    {
    }

    [[nodiscard]] bool map(vaddr_t virtual_address, paddr_t physical_address, page_size size,
                           page_flags flags) const
    {
        table_node node = root();

        if (!node.valid())
            return false;

        const bool user = (flags.word(0) & PAGE_FLAG_USER) != 0;

        while (!node.maps(size)) {
            u64 &slot = node.entry(virtual_address);

            if (present(slot) && large(slot) && !node.split_large_leaf(slot))
                return false;

            table_node child;

            if (!node.ensure_child(virtual_address, user, child))
                return false;

            node = child;
        }

        return node.set_leaf(virtual_address, physical_address, size, flags);
    }

    [[nodiscard]] leaf_location find_leaf(vaddr_t virtual_address) const
    {
        table_node node = root();

        while (node.valid()) {
            u64 &slot = node.entry(virtual_address);

            if (!present(slot))
                return {};

            if (node.level() == table_level::PML4) {
                if (large(slot))
                    return {};

                node = node.child(slot);
                continue;
            }

            if (node.level() == table_level::PT || large(slot))
                return {.entry = &slot, .size = page_size_at(node.level())};

            node = node.child(slot);
        }

        return {};
    }

    [[nodiscard]] bool split_leaf(vaddr_t virtual_address, page_size size) const
    {
        table_node node = root();

        while (node.valid()) {
            u64 &slot = node.entry(virtual_address);

            if (!present(slot))
                return false;

            if (node.maps(size))
                return node.split_large_leaf(slot);

            if (large(slot))
                return false;

            node = node.child(slot);
        }

        return false;
    }

    void reclaim_empty_tables(vaddr_t virtual_address) const
    {
        const table_path path = path_to(virtual_address);

        // Deepest first: a table can only be collected once the one below it is gone, so the first
        // level that is still holding something stops the unwind.
        for (usize depth = path.depth; depth > 0;) {
            u64          &slot = *path.entries[--depth];
            const paddr_t physical = entry_address(slot);
            page_table   *table = table_from_physical(physical);

            if (table == nullptr || !table_is_empty(*table) || !table_is_reclaimable(physical))
                return;

            slot = 0;
            kernel::core::memory::pmm::span_4k::free(physical);
        }
    }
};

[[nodiscard]] u64 page_offset(vaddr_t address, page_size size)
{
    return address & (bytes_in(size) - 1);
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
    // A size the CPU cannot represent as a leaf is not a slow mapping, it is a reserved-bit fault
    // on first touch. The size is part of the public API, so the check belongs here rather than in
    // whoever happened to pick it.
    if (is_hw_allocatable(size) != page_hw_allocatability::YES)
        return false;

    const u64 page_bytes = bytes_in(size);

    if (!valid() || !is_canonical(virtual_address) ||
        !is_aligned<vaddr_t>(virtual_address, page_bytes) ||
        !is_aligned<paddr_t>(physical_address, page_bytes))
        return false;

    return page_table_tree{root_m}.map(virtual_address, physical_address, size, flags);
}

bool address_space::protect(vaddr_t virtual_address, page_size size, page_flags flags)
{
    if (!valid() || !is_canonical(virtual_address) ||
        !is_aligned<vaddr_t>(virtual_address, bytes_in(size)))
        return false;

    const leaf_location leaf = page_table_tree{root_m}.find_leaf(virtual_address);

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
    const page_table_tree tree{root_m};

    for (;;) {
        const leaf_location leaf = tree.find_leaf(virtual_address);

        if (leaf.entry == nullptr)
            return false;

        if (leaf.size == size) {
            *leaf.entry = 0;
            tree.reclaim_empty_tables(virtual_address);
            return true;
        }

        if (bytes_in(leaf.size) < bytes_in(size))
            return false;

        if (!tree.split_leaf(virtual_address, leaf.size))
            return false;
    }
}

mapping address_space::translate(vaddr_t virtual_address) const
{
    if (!valid() || !is_canonical(virtual_address))
        return {};

    const leaf_location leaf = page_table_tree{root_m}.find_leaf(virtual_address);

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
