#ifndef DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_TLB_HPP_
#define DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_TLB_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"

namespace kernel::core::memory::vmm::mmu::tlb {

using kernel::core::vaddr_t;

void flush(vaddr_t vaddr);
void flush_all();
void flush_all_global();

}  // namespace kernel::core::memory::vmm::mmu::tlb

#endif  // DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_TLB_HPP_
