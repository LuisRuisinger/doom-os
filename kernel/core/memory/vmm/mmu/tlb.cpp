// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/memory/vmm/mmu/tlb.hpp"

#include "kernel/arch/x86_64/cpu/registers.hpp"

namespace kernel::core::memory::vmm::mmu::tlb {

namespace regs = kernel::arch::x86_64::cpu;

void flush(vaddr_t virtual_address)
{
    asm volatile("invlpg (%0)" : : "r"(virtual_address) : "memory");
}

void flush_all()
{
    regs::write_cr3(regs::read_cr3());
}

void flush_all_global()
{
    const kernel::core::u64 cr4 = regs::read_cr4();

    // Toggling PGE is the only way to evict global entries, and it evicts nothing unless the bit
    // actually changes: writing CR4 back unchanged is not required to invalidate anything. With
    // PGE off this has to fall through to a CR3 reload rather than silently doing nothing - and
    // it is the only flush after the kernel image is re-protected.
    if ((cr4 & regs::CR4_PAGE_GLOBAL_ENABLE) == 0) {
        flush_all();
        return;
    }

    regs::write_cr4(cr4 & ~regs::CR4_PAGE_GLOBAL_ENABLE);
    regs::write_cr4(cr4);
}

}  // namespace kernel::core::memory::vmm::mmu::tlb
