#ifndef DOOM_OS_ARCH_X86_64_MMU_PAGING_HPP_
#define DOOM_OS_ARCH_X86_64_MMU_PAGING_HPP_

#include "arch/common/mmu/traits.hpp"
#include "arch/x86_64/mmu/features.hpp"
#include "kernel/core/types.hpp"
#include "kernel/mm/page.hpp"

namespace kernel::arch::x86_64::mmu {

using kernel::core::paddr_t;
using kernel::core::u64;
using kernel::core::usize;
using kernel::core::vaddr_t;
using kernel::mm::bytes_in;
using kernel::mm::has;
using kernel::mm::page_prot;
using kernel::mm::page_size;

struct paging : kernel::arch::common::mmu::radix9<4> {
    static constexpr u64 PRESENT = u64{1} << 0;
    static constexpr u64 WRITABLE = u64{1} << 1;
    static constexpr u64 USER = u64{1} << 2;
    static constexpr u64 CACHE_DISABLE = u64{1} << 4;
    static constexpr u64 LARGE = u64{1} << 7;
    static constexpr u64 GLOBAL = u64{1} << 8;
    static constexpr u64 NO_EXECUTE = u64{1} << 63;
    static constexpr u64 ADDRESS_MASK = 0x000FFFFFFFFFF000;
    static constexpr u64 PROT_MASK = WRITABLE | USER | CACHE_DISABLE | GLOBAL | NO_EXECUTE;

    [[nodiscard]] static constexpr bool valid(vaddr_t va)
    {
        return (va >> 47) == 0 || (va >> 47) == 0x1FFFF;
    }

    [[nodiscard]] static bool supports(page_size size)
    {
        return size != page_size::SIZE_1G || gib_pages_supported();
    }

    [[nodiscard]] static constexpr bool present(u64 entry)
    {
        return (entry & PRESENT) != 0;
    }

    [[nodiscard]] static constexpr bool leaf(u64 entry, usize level)
    {
        return level == 1 || (entry & LARGE) != 0;
    }

    [[nodiscard]] static constexpr paddr_t address(u64 entry)
    {
        return entry & ADDRESS_MASK;
    }

    [[nodiscard]] static constexpr page_prot prot(u64 entry)
    {
        page_prot prot{};

        if ((entry & WRITABLE) != 0)
            prot |= page_prot::WRITE;
        if ((entry & NO_EXECUTE) == 0)
            prot |= page_prot::EXEC;
        if ((entry & USER) != 0)
            prot |= page_prot::USER;
        if ((entry & GLOBAL) != 0)
            prot |= page_prot::GLOBAL;
        if ((entry & CACHE_DISABLE) != 0)
            prot |= page_prot::UNCACHED;

        return prot;
    }

    [[nodiscard]] static constexpr u64 table_entry(paddr_t phys)
    {
        return (phys & ADDRESS_MASK) | PRESENT | WRITABLE | USER;
    }

    [[nodiscard]] static u64 leaf_entry(paddr_t phys, page_size size, page_prot prot)
    {
        u64 entry = (phys & ADDRESS_MASK) | PRESENT;

        if (size != page_size::SIZE_4K)
            entry |= LARGE;
        if (has(prot, page_prot::WRITE))
            entry |= WRITABLE;
        if (has(prot, page_prot::USER))
            entry |= USER;
        if (has(prot, page_prot::UNCACHED))
            entry |= CACHE_DISABLE;
        if (has(prot, page_prot::GLOBAL) && global_pages_enabled())
            entry |= GLOBAL;
        if (!has(prot, page_prot::EXEC) && nx_enabled())
            entry |= NO_EXECUTE;

        return entry;
    }

    static void split(u64 *child, u64 entry, usize level)
    {
        const u64 step = bytes_in(size_at(level - 1));
        const u64 flags = (entry & (PRESENT | PROT_MASK)) | (level - 1 == 1 ? 0 : LARGE);

        for (usize i = 0; i < ENTRIES; ++i)
            child[i] = ((address(entry) + i * step) & ADDRESS_MASK) | flags;
    }
};

static_assert(kernel::arch::common::mmu::page_format<paging>);

}  // namespace kernel::arch::x86_64::mmu

#endif  // DOOM_OS_ARCH_X86_64_MMU_PAGING_HPP_
