#ifndef DOOM_OS_ARCH_X86_64_MMU_MMU_HPP_
#define DOOM_OS_ARCH_X86_64_MMU_MMU_HPP_

#include "arch/common/mmu/radix_tree.hpp"
#include "arch/x86_64/mmu/paging.hpp"
#include "arch/x86_64/mmu/tlb.hpp"
#include "config/layout.h"
#include "kernel/core/cast.hpp"
#include "kernel/core/result.hpp"
#include "kernel/core/types.hpp"
#include "kernel/init/component.hpp"
#include "kernel/mm/page.hpp"
#include "kernel/mm/pmm.hpp"

extern "C" char kernel_physical_start[];
extern "C" char kernel_physical_end[];

namespace kernel::arch::x86_64::mmu {

using kernel::core::Result;
using kernel::core::uptr;
using kernel::mm::mm_error;

inline constexpr vaddr_t KERNEL_BASE = DOOM_OS_KERNEL_VMA;
inline constexpr vaddr_t DIRECT_MAP_BASE = DOOM_OS_DIRECT_MAP_BASE;
inline constexpr u64     DIRECT_MAP_SIZE = DOOM_OS_MAX_PHYSICAL_MEMORY;
inline constexpr u64     EARLY_MAP_SIZE = DOOM_OS_EARLY_MAP_SIZE;
inline constexpr vaddr_t MMIO_MAP_BASE = DOOM_OS_MMIO_MAP_BASE;
inline constexpr u64     MMIO_MAP_SIZE = DOOM_OS_MMIO_MAP_SIZE;

[[nodiscard]] inline void *phy_to_vrt(paddr_t phys)
{
    return phys < DIRECT_MAP_SIZE ? (DIRECT_MAP_BASE + phys) as(void *) : nullptr;
}

struct kernel_env {
    [[nodiscard]] static u64 *at(paddr_t phys)
    {
        return (DIRECT_MAP_BASE + phys) as(u64 *);
    }

    [[nodiscard]] static Result<paddr_t, mm_error> alloc_table()
    {
        return kernel::mm::pmm::alloc().map([](paddr_t phys) {
            u64 *table = at(phys);

            for (usize i = 0; i < paging::ENTRIES; ++i)
                table[i] = 0;

            return phys;
        });
    }

    static void free_table(paddr_t phys)
    {
        const auto image_first = kernel_physical_start as(uptr);
        const auto image_end = kernel_physical_end as(uptr);

        if (phys < image_first || phys >= image_end)
            kernel::mm::pmm::free(page_size::SIZE_4K, phys);
    }

    static void replace(u64 &slot, u64 entry, vaddr_t va)
    {
        const u64 old = slot;

        slot = entry;

        if (paging::present(old))
            tlb::flush(va);
    }
};

static_assert(kernel::arch::common::mmu::page_env<kernel_env>);

using address_space = kernel::arch::common::mmu::radix_tree<paging, kernel_env>;

[[nodiscard]] address_space &kernel_space();

struct component
    : kernel::init::component<component, kernel::init::no_resource, kernel::mm::pmm::component> {
    static constexpr auto *name = "MMU";

    static kernel::init::init_result init_kernel_space();

    template <typename View>
    static kernel::init::init_result init(View)
    {
        return init_kernel_space();
    }
};

}  // namespace kernel::arch::x86_64::mmu

#endif  // DOOM_OS_ARCH_X86_64_MMU_MMU_HPP_
