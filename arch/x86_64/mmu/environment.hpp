#ifndef DOOM_OS_ARCH_X86_64_MMU_ENVIRONMENT_HPP_
#define DOOM_OS_ARCH_X86_64_MMU_ENVIRONMENT_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "arch/common/mmu/hw_radix_tree.hpp"
#include "arch/x86_64/mmu/direct_map.hpp"
#include "kernel/core/types.hpp"
#include "kernel/mm/pmm.hpp"

namespace kernel::arch::x86_64::mmu {

using kernel::core::paddr_t;
using kernel::core::u64;

struct default_paging_environment {
    static constexpr paddr_t INVALID_ADDRESS = kernel::mm::pmm::INVALID_PHYSICAL_ADDRESS;

    [[nodiscard]] static paddr_t allocate_table()
    {
        if (!direct_map_ready())
            return INVALID_ADDRESS;

        const paddr_t phys = kernel::mm::pmm::span_4k::alloc();
        if (phys == INVALID_ADDRESS)
            return INVALID_ADDRESS;

        u64 *table = map_table(phys);
        for (usize i = 0; i < 512; ++i)
            table[i] = 0;

        return phys;
    }

    static void free_table(paddr_t phys)
    {
        kernel::mm::pmm::span_4k::free(phys);
    }

    [[nodiscard]] static u64 *map_table(paddr_t phys)
    {
        return static_cast<u64 *>(physical_window(phys));
    }

    [[nodiscard]] static bool is_reclaimable(paddr_t phys);
};

static_assert(kernel::arch::common::mmu::paging_environment<default_paging_environment>);

void set_reserved_table_range(paddr_t first, paddr_t last);

}  // namespace kernel::arch::x86_64::mmu

#endif  // DOOM_OS_ARCH_X86_64_MMU_ENVIRONMENT_HPP_