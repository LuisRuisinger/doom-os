#ifndef DOOM_OS_ARCH_COMMON_MMU_TRAITS_HPP_
#define DOOM_OS_ARCH_COMMON_MMU_TRAITS_HPP_

#include <concepts>

#include "kernel/core/cast.hpp"
#include "kernel/core/result.hpp"
#include "kernel/core/types.hpp"
#include "kernel/mm/page.hpp"

namespace kernel::arch::common::mmu {

using kernel::core::paddr_t;
using kernel::core::Result;
using kernel::core::u64;
using kernel::core::usize;
using kernel::core::vaddr_t;
using kernel::mm::mm_error;
using kernel::mm::page_prot;
using kernel::mm::page_size;
using kernel::mm::shift_of;

template <usize Levels>
struct radix9 {
    static constexpr usize LEVELS = Levels;
    static constexpr usize ENTRIES = 512;

    [[nodiscard]] static constexpr usize index(usize level, vaddr_t va)
    {
        return ((va >> (3 + 9 * level)) & 0x1FF) as(usize);
    }

    [[nodiscard]] static constexpr usize level_of(page_size size)
    {
        return ((shift_of(size) - 12) / 9 + 1) as(usize);
    }

    [[nodiscard]] static constexpr page_size size_at(usize level)
    {
        return (12 + 9 * (level - 1)) as(page_size);
    }
};

template <class F>
concept page_format = requires(u64 entry, usize level, vaddr_t va, paddr_t pa, page_size size,
                               page_prot prot, u64 *table) {
    { F::LEVELS } -> std::convertible_to<usize>;
    { F::ENTRIES } -> std::convertible_to<usize>;
    { F::index(level, va) } -> std::same_as<usize>;
    { F::level_of(size) } -> std::same_as<usize>;
    { F::size_at(level) } -> std::same_as<page_size>;
    { F::valid(va) } -> std::same_as<bool>;
    { F::supports(size) } -> std::same_as<bool>;
    { F::present(entry) } -> std::same_as<bool>;
    { F::leaf(entry, level) } -> std::same_as<bool>;
    { F::address(entry) } -> std::same_as<paddr_t>;
    { F::prot(entry) } -> std::same_as<page_prot>;
    { F::table_entry(pa) } -> std::same_as<u64>;
    { F::leaf_entry(pa, size, prot) } -> std::same_as<u64>;
    { F::split(table, entry, level) } -> std::same_as<void>;
};

template <class E>
concept page_env = requires(paddr_t pa, vaddr_t va, u64 &slot, u64 entry) {
    { E::at(pa) } -> std::same_as<u64 *>;
    { E::alloc_table() } -> std::same_as<Result<paddr_t, mm_error>>;
    { E::free_table(pa) } -> std::same_as<void>;
    { E::replace(slot, entry, va) } -> std::same_as<void>;
};

}  // namespace kernel::arch::common::mmu

#endif  // DOOM_OS_ARCH_COMMON_MMU_TRAITS_HPP_
