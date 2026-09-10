#ifndef DOOM_OS_ARCH_X86_64_MMU_TRAITS_HPP_
#define DOOM_OS_ARCH_X86_64_MMU_TRAITS_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "arch/common/mmu/traits.hpp"
#include "arch/x86_64/mmu/entry.hpp"
#include "arch/x86_64/mmu/pagetable.hpp"
#include "kernel/core/types.hpp"
#include "kernel/mm/page.hpp"

namespace kernel::arch::x86_64::mmu {

using kernel::core::paddr_t;
using kernel::core::u64;
using kernel::core::usize;
using kernel::core::vaddr_t;
using kernel::mm::page_size;

struct x86_64_paging_traits : kernel::arch::common::mmu::radix9_geometry {
    using flags_type = page_flags;

    static constexpr usize MAX_LEVEL = 4;

    [[nodiscard]] static constexpr page_size page_size_at(usize level)
    {
        switch (level) {
            case 3: return page_size::SIZE_1G;
            case 2: return page_size::SIZE_2M;
            case 1: return page_size::SIZE_4K;
            default: return page_size::SIZE_1G;
        }
    }

    [[nodiscard]] static constexpr bool maps_page_size(usize level, page_size size)
    {
        return (level == 3 && size == page_size::SIZE_1G) ||
               (level == 2 && size == page_size::SIZE_2M) ||
               (level == 1 && size == page_size::SIZE_4K);
    }

    [[nodiscard]] static bool is_present(u64 entry)
    {
        return present(entry);
    }

    [[nodiscard]] static bool is_leaf(u64 entry, usize level)
    {
        return level == 1 || large(entry);
    }

    [[nodiscard]] static bool is_user(u64 entry)
    {
        return (entry & PAGE_FLAG_USER) != 0;
    }

    [[nodiscard]] static bool is_user_flags(page_flags flags)
    {
        return (flags.word(0) & PAGE_FLAG_USER) != 0;
    }

    [[nodiscard]] static paddr_t entry_address(u64 entry)
    {
        return mmu::entry_address(entry);
    }

    [[nodiscard]] static u64 make_table_entry(paddr_t child_phys, bool user)
    {
        return table_entry(child_phys, user);
    }

    [[nodiscard]] static u64 make_leaf_entry(paddr_t target_phys, page_size size, page_flags flags)
    {
        return leaf_entry(target_phys, size, flags);
    }

    static void ensure_intermediate_permissions(u64 &slot)
    {
        slot |= PAGE_FLAG_WRITABLE;
    }

    [[nodiscard]] static page_flags flags_from_entry(u64 entry)
    {
        return mmu::flags_from_entry(entry);
    }

    static void populate_split(u64 *child_table, u64 old_leaf_entry, usize parent_level)
    {
        const usize child_level = parent_level - 1;
        const u64   child_step  = span_at(child_level);
        const u64   child_large = child_level == 2 ? ENTRY_LARGE : 0;
        const u64   child_flags = old_leaf_entry & ENTRY_INHERITED_FLAGS;
        const paddr_t phys      = entry_address(old_leaf_entry);

        for (usize i = 0; i < ENTRIES_PER_TABLE; ++i) {
            child_table[i] = ((phys + i * child_step) & ENTRY_ADDRESS_MASK) | child_flags | child_large;
        }
    }
};

static_assert(kernel::arch::common::mmu::paging_traits<x86_64_paging_traits>);

}  // namespace kernel::arch::x86_64::mmu

#endif  // DOOM_OS_ARCH_X86_64_MMU_TRAITS_HPP_