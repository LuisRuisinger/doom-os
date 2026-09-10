#ifndef DOOM_OS_ARCH_X86_64_MMU_MMU_HPP_
#define DOOM_OS_ARCH_X86_64_MMU_MMU_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/init/component.hpp"
#include "kernel/mm/pmm.hpp"
#include "arch/x86_64/mmu/direct_map.hpp"
#include "arch/x86_64/mmu/pagetable.hpp"
#include "kernel/core/types.hpp"

namespace kernel::arch::x86_64::mmu {

using kernel::init::init_result;
using kernel::core::paddr_t;
using kernel::core::u64;
using kernel::core::vaddr_t;

// =================================================================================================
// Active kernel address space
// =================================================================================================

[[nodiscard]] address_space &kernel_address_space();

[[nodiscard]] init_result init_kernel_address_space();

[[nodiscard]] bool map(vaddr_t virtual_address, paddr_t physical_address, page_size size,
                       page_flags flags);
[[nodiscard]] bool map_range(vaddr_t virtual_address, paddr_t physical_address, u64 length,
                             page_size size, page_flags flags);
[[nodiscard]] bool unmap(vaddr_t virtual_address, page_size size);
[[nodiscard]] mapping vrt_to_phy(vaddr_t virtual_address);

// =================================================================================================
// Component
// =================================================================================================

struct component : kernel::init::component<component, kernel::init::no_resource,
                                           kernel::mm::pmm::component> {
    static constexpr auto *name = "MMU";

    template <typename View>
    static kernel::init::init_result init(View)
    {
        return init_kernel_address_space();
    }
};

}  // namespace kernel::arch::x86_64::mmu

#endif  // DOOM_OS_ARCH_X86_64_MMU_MMU_HPP_
