#ifndef DOOM_OS_KERNEL_ARCH_X86_64_CORE_INIT_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_CORE_INIT_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/cpu.hpp"
#include "kernel/boot/component.hpp"

namespace kernel::arch::x86_64::core_init {
// =================================================================================================
// Core initialization
// =================================================================================================

bool init_core(cpu::local_state &cpu);

bool init_bsp();

// =================================================================================================
// Component
// =================================================================================================

struct component
    : kernel::boot::component_base<component,
                                   kernel::boot::type_list<kernel::arch::x86_64::cpu::component> > {
    static constexpr const char *name = "CORE_INIT";

    static bool init_component() { return init_bsp(); }
};
}  // namespace kernel::arch::x86_64::core_init

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_CORE_INIT_HPP_