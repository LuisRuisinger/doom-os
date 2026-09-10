#ifndef DOOM_OS_ARCH_COMMON_MMU_HW_RADIX_TREE_HPP_
#define DOOM_OS_ARCH_COMMON_MMU_HW_RADIX_TREE_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "arch/common/mmu/traits.hpp"
#include "kernel/core/types.hpp"
#include "kernel/mm/page.hpp"

namespace kernel::arch::common::mmu {

using kernel::core::paddr_t;
using kernel::core::u64;
using kernel::core::usize;
using kernel::core::vaddr_t;
using kernel::mm::bytes_in;
using kernel::mm::page_size;

template <paging_traits Traits, paging_environment Environment>
class hw_radix_tree {
    paddr_t m_root{};

    struct leaf_location {
        u64      *entry{};
        page_size size{};
    };

    struct table_path {
        u64  *entries[Traits::MAX_LEVEL - 1]{};
        usize depth{};

        void push(u64 &entry)
        {
            entries[depth++] = &entry;
        }
    };

    [[nodiscard]] static bool is_empty(const u64 *table)
    {
        for (usize i = 0; i < Traits::ENTRIES_PER_TABLE; ++i) {
            if (table[i] != 0)
                return false;
        }
        return true;
    }

    [[nodiscard]] table_path path_to(vaddr_t vaddr) const
    {
        table_path path{};
        paddr_t current_phys = m_root;

        for (usize level = Traits::MAX_LEVEL; level > 1; --level) {
            u64 *table = Environment::map_table(current_phys);
            if (table == nullptr)
                break;

            u64 &slot = table[Traits::index_at(level, vaddr)];
            if (!Traits::is_present(slot) || Traits::is_leaf(slot, level))
                break;

            path.push(slot);
            current_phys = Traits::entry_address(slot);
        }

        return path;
    }

    [[nodiscard]] leaf_location find_leaf(vaddr_t vaddr) const
    {
        paddr_t current_phys = m_root;

        for (usize level = Traits::MAX_LEVEL; level >= 1; --level) {
            u64 *table = Environment::map_table(current_phys);
            if (table == nullptr)
                return {};

            u64 &slot = table[Traits::index_at(level, vaddr)];
            if (!Traits::is_present(slot))
                return {};

            if (level == 1 || Traits::is_leaf(slot, level))
                return {.entry = &slot, .size = Traits::page_size_at(level)};

            current_phys = Traits::entry_address(slot);
        }

        return {};
    }

    [[nodiscard]] bool split_leaf_at(vaddr_t vaddr, page_size size) const
    {
        paddr_t current_phys = m_root;

        for (usize level = Traits::MAX_LEVEL; level >= 1; --level) {
            u64 *table = Environment::map_table(current_phys);
            if (table == nullptr)
                return false;

            u64 &slot = table[Traits::index_at(level, vaddr)];
            if (!Traits::is_present(slot))
                return false;

            if (Traits::maps_page_size(level, size)) {
                if (!Traits::is_leaf(slot, level))
                    return false;

                const paddr_t child_phys = Environment::allocate_table();
                if (child_phys == Environment::INVALID_ADDRESS)
                    return false;

                u64 *child_table = Environment::map_table(child_phys);
                Traits::populate_split(child_table, slot, level);
                slot = Traits::make_table_entry(child_phys, Traits::is_user(slot));
                return true;
            }

            if (Traits::is_leaf(slot, level))
                return false;

            current_phys = Traits::entry_address(slot);
        }

        return false;
    }

    void reclaim_empty_tables(vaddr_t vaddr) const
    {
        const table_path path = path_to(vaddr);

        for (usize depth = path.depth; depth > 0;) {
            u64          &slot = *path.entries[--depth];
            const paddr_t phys = Traits::entry_address(slot);
            const u64    *table = Environment::map_table(phys);

            if (table == nullptr || !is_empty(table) || !Environment::is_reclaimable(phys))
                return;

            slot = 0;
            Environment::free_table(phys);
        }
    }

public:
    explicit constexpr hw_radix_tree(paddr_t root)
        : m_root(root)
    {
    }

    [[nodiscard]] bool map(vaddr_t vaddr, paddr_t paddr, page_size size, typename Traits::flags_type flags) const
    {
        paddr_t current_phys = m_root;
        const bool user = Traits::is_user_flags(flags);

        for (usize level = Traits::MAX_LEVEL; level >= 1; --level) {
            u64 *table = Environment::map_table(current_phys);
            if (table == nullptr)
                return false;

            u64 &slot = table[Traits::index_at(level, vaddr)];

            if (Traits::maps_page_size(level, size)) {
                if (Traits::is_present(slot) && Traits::is_leaf(slot, level) != (size != page_size::SIZE_4K))
                    return false;

                slot = Traits::make_leaf_entry(paddr, size, flags);
                return true;
            }

            if (Traits::is_present(slot)) {
                if (Traits::is_leaf(slot, level)) {
                    const paddr_t split_frame = Environment::allocate_table();
                    if (split_frame == Environment::INVALID_ADDRESS)
                        return false;

                    u64 *split_table = Environment::map_table(split_frame);
                    Traits::populate_split(split_table, slot, level);
                    slot = Traits::make_table_entry(split_frame, Traits::is_user(slot));
                }

                if (user != Traits::is_user(slot))
                    return false;

                Traits::ensure_intermediate_permissions(slot);
            } else {
                const paddr_t child_phys = Environment::allocate_table();
                if (child_phys == Environment::INVALID_ADDRESS)
                    return false;

                slot = Traits::make_table_entry(child_phys, user);
            }

            current_phys = Traits::entry_address(slot);
        }

        return false;
    }

    [[nodiscard]] bool protect(vaddr_t vaddr, page_size size, typename Traits::flags_type flags) const
    {
        const leaf_location leaf = find_leaf(vaddr);
        if (leaf.entry == nullptr || leaf.size != size)
            return false;

        *leaf.entry = Traits::make_leaf_entry(Traits::entry_address(*leaf.entry), size, flags);
        return true;
    }

    [[nodiscard]] bool unmap(vaddr_t vaddr, page_size size) const
    {
        for (;;) {
            const leaf_location leaf = find_leaf(vaddr);
            if (leaf.entry == nullptr)
                return false;

            if (leaf.size == size) {
                *leaf.entry = 0;
                reclaim_empty_tables(vaddr);
                return true;
            }

            if (bytes_in(leaf.size) < bytes_in(size))
                return false;

            if (!split_leaf_at(vaddr, leaf.size))
                return false;
        }
    }

    template <typename MappingType>
    [[nodiscard]] MappingType translate(vaddr_t vaddr) const
    {
        const leaf_location leaf = find_leaf(vaddr);
        if (leaf.entry == nullptr)
            return {};

        return MappingType{
            .present = true,
            .frame = Traits::entry_address(*leaf.entry),
            .offset = vaddr & (bytes_in(leaf.size) - 1),
            .size = leaf.size,
            .flags = Traits::flags_from_entry(*leaf.entry),
        };
    }
};

}  // namespace kernel::arch::common::mmu

#endif  // DOOM_OS_ARCH_COMMON_MMU_HW_RADIX_TREE_HPP_