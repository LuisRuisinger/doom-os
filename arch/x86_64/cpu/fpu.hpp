#ifndef DOOM_OS_KERNEL_ARCH_X86_64_CPU_FPU_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_CPU_FPU_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/init/component.hpp"

namespace kernel::arch::x86_64::cpu {

// =================================================================================================
// FPU and SSE
//
// The kernel is compiled -mno-sse -mno-80387 and never touches x87 or XMM. This component is not
// for the kernel's benefit: it is what makes those instructions legal for the application and for
// any driver that opts into them, which cannot work while CR0.EM leaves them trapping.
//
// Enabled per core, because CR0 and CR4 are per core and a core that skipped this would fault on
// the first XMM instruction the application executed on it.
//
// The kernel abstaining is what keeps this cheap. Interrupt and exception handlers are kernel
// code, so they cannot clobber XMM, so there is no register state to save on entry and restore on
// exit - no FXSAVE area, no lazy-switch dance. A driver that uses SSE inside an interrupt handler
// breaks exactly that, and would corrupt whatever the interrupted code had in XMM.
// =================================================================================================

void enable_sse();

struct fpu_component : kernel::init::component<fpu_component, kernel::init::no_resource> {
    static constexpr auto *name = "FPU";

    template <typename View>
    static kernel::init::init_result init(View)
    {
        enable_sse();

        return kernel::core::Ok();
    }
};

}  // namespace kernel::arch::x86_64::cpu

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_CPU_FPU_HPP_
