
#include "arch/x86_64/cpu/fpu.hpp"

#include "arch/x86_64/cpu/registers.hpp"

namespace kernel::arch::x86_64::cpu {

void enable_sse()
{
    u64 cr0 = read_cr0();

    cr0 &= ~CR0_EMULATION;
    cr0 |= CR0_MONITOR_COPROCESSOR;

    write_cr0(cr0);

    write_cr4(read_cr4() | CR4_OS_FXSR | CR4_OS_XMM_EXCEPT);

    asm volatile("fninit" ::: "memory");
}

}  // namespace kernel::arch::x86_64::cpu
