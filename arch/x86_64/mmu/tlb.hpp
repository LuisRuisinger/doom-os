#ifndef DOOM_OS_ARCH_X86_64_MMU_TLB_HPP_
#define DOOM_OS_ARCH_X86_64_MMU_TLB_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"

namespace kernel::arch::x86_64::mmu::tlb {

using kernel::core::vaddr_t;

void flush(vaddr_t vaddr);
void flush_all();
void flush_all_global();

}  // namespace kernel::arch::x86_64::mmu::tlb

#endif  // DOOM_OS_ARCH_X86_64_MMU_TLB_HPP_
