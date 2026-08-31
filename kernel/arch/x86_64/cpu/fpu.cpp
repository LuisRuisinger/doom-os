// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/cpu/fpu.hpp"

#include "kernel/arch/x86_64/cpu/registers.hpp"

namespace kernel::arch::x86_64::cpu {

void enable_sse()
{
    // EM makes x87 instructions trap so software can emulate them; there is no emulator here and
    // it also has to be clear for SSE. MP pairs with TS to make WAIT trap, which is the classic
    // lazy-switch mechanism - kept set because that is the architecturally defined pairing, even
    // though nothing sets TS.
    u64 cr0 = read_cr0();

    cr0 &= ~CR0_EMULATION;
    cr0 |= CR0_MONITOR_COPROCESSOR;

    write_cr0(cr0);

    // OSFXSR tells the CPU this OS is prepared to save XMM with FXSAVE, which is what actually
    // un-traps SSE. OSXMMEXCPT routes SIMD floating-point errors to #XM rather than #UD, so a
    // division by zero in application code reports as itself.
    write_cr4(read_cr4() | CR4_OS_FXSR | CR4_OS_XMM_EXCEPT);

    // The x87 state after reset is not the state a C runtime expects; newlib's long double paths
    // read the control word before writing it.
    asm volatile("fninit" ::: "memory");
}

}  // namespace kernel::arch::x86_64::cpu
