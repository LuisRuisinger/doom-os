// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/memory/vmm/mmu/tlb.hpp"

#include "kernel/arch/x86_64/cpu/registers.hpp"

namespace kernel::core::memory::vmm::mmu::tlb {

namespace regs = kernel::arch::x86_64::cpu;

void flush(vaddr_t vaddr)
{
    asm volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

void flush_all()
{
    regs::write_cr3(regs::read_cr3());
}

void flush_all_global()
{
    const kernel::core::u64 cr4 = regs::read_cr4();
    if ((cr4 & regs::CR4_PAGE_GLOBAL_ENABLE) == 0) {
        flush_all();
        return;
    }

    regs::write_cr4(cr4 & ~regs::CR4_PAGE_GLOBAL_ENABLE);
    regs::write_cr4(cr4);
}

}  // namespace kernel::core::memory::vmm::mmu::tlb
