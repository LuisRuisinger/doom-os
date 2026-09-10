#ifndef DOOM_OS_ARCH_COMMON_MMU_TRAITS_HPP_
#define DOOM_OS_ARCH_COMMON_MMU_TRAITS_HPP_

// =================================================================================================
// Cpp stdlib files
// =================================================================================================

#include <concepts>

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"
#include "kernel/mm/page.hpp"

namespace kernel::arch::common::mmu {

using kernel::core::paddr_t;
using kernel::core::u64;
using kernel::core::usize;
using kernel::core::vaddr_t;
using kernel::mm::page_size;

// =================================================================================================
// Common 9-bit Radix Geometry (Shared by x86_64, AArch64 4K, and RISC-V Sv48)
// =================================================================================================

struct radix9_geometry {
    static constexpr usize ENTRIES_PER_TABLE = 512; //

    [[nodiscard]] static constexpr usize index_at(usize level, vaddr_t vaddr)
    {
        // Level 1 = shift 12, Level 2 = shift 21, Level 3 = shift 30, Level 4 = shift 39
        const usize shift = 12 + (level - 1) * 9;
        return static_cast<usize>((vaddr >> shift) & 0x1FF); //
    }

    [[nodiscard]] static constexpr u64 span_at(usize level)
    {
        return u64{1} << (12 + (level - 1) * 9);
    }
};

// =================================================================================================
// Concepts
// =================================================================================================

template <typename T>
concept paging_traits = requires(u64 entry, usize level, vaddr_t vaddr, paddr_t paddr,
                                 page_size size, typename T::flags_type flags, u64 *table) {
    { T::MAX_LEVEL } -> std::convertible_to<usize>;
    { T::ENTRIES_PER_TABLE } -> std::convertible_to<usize>;

    { T::index_at(level, vaddr) } -> std::same_as<usize>;
    { T::span_at(level) } -> std::same_as<u64>;
    { T::page_size_at(level) } -> std::same_as<page_size>;
    { T::maps_page_size(level, size) } -> std::same_as<bool>;

    { T::is_present(entry) } -> std::same_as<bool>;
    { T::is_leaf(entry, level) } -> std::same_as<bool>;
    { T::is_user(entry) } -> std::same_as<bool>;
    { T::is_user_flags(flags) } -> std::same_as<bool>;
    { T::entry_address(entry) } -> std::same_as<paddr_t>;

    { T::make_table_entry(paddr, true) } -> std::same_as<u64>;
    { T::make_leaf_entry(paddr, size, flags) } -> std::same_as<u64>;
    { T::ensure_intermediate_permissions(entry) } -> std::same_as<void>;
    { T::populate_split(table, entry, level) } -> std::same_as<void>;
    { T::flags_from_entry(entry) } -> std::same_as<typename T::flags_type>;
};

template <typename E>
concept paging_environment = requires(paddr_t paddr) {
    { E::INVALID_ADDRESS } -> std::convertible_to<paddr_t>;
    { E::allocate_table() } -> std::same_as<paddr_t>;
    { E::free_table(paddr) } -> std::same_as<void>;
    { E::map_table(paddr) } -> std::same_as<u64 *>;
    { E::is_reclaimable(paddr) } -> std::same_as<bool>;
};

}  // namespace kernel::arch::common::mmu

#endif  // DOOM_OS_ARCH_COMMON_MMU_TRAITS_HPP_