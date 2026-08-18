#ifndef DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_MMU_HPP_
#define DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_MMU_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/component.hpp"
#include "kernel/core/memory/pmm/pmm.hpp"
#include "kernel/core/memory/vmm/mmu/direct_map.hpp"
#include "kernel/core/memory/vmm/mmu/features.hpp"
#include "kernel/core/memory/vmm/mmu/pagetable.hpp"
#include "kernel/core/types.hpp"

namespace kernel::core::memory::vmm::mmu {

using kernel::core::init_result;
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
[[nodiscard]] mapping translate(vaddr_t virtual_address);

// =================================================================================================
// Component
// =================================================================================================

struct component : kernel::core::component<component, kernel::core::no_resource,
                                           kernel::core::memory::pmm::component> {
    static constexpr auto *name = "MMU";

    template <typename View>
    static kernel::core::init_result init(View)
    {
        return init_kernel_address_space();
    }
};

}  // namespace kernel::core::memory::vmm::mmu

#endif  // DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_MMU_HPP_
