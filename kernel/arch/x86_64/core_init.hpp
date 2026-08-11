#ifndef DOOM_OS_KERNEL_ARCH_X86_64_CORE_INIT_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_CORE_INIT_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/cpu/cpu.hpp"
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

struct component : kernel::boot::component<component, kernel::boot::no_resource,
                                           kernel::arch::x86_64::cpu::component> {
    static constexpr auto *name = "CORE_INIT";

    template <typename View>
    static kernel::boot::init_result init(View)
    {
        // The per-core graph logs whichever component failed and why, so there is nothing
        // more specific to add here.
        if (!init_bsp()) {
            return kernel::boot::Err(kernel::boot::init_error::UNSPECIFIED);
        }

        return kernel::boot::Ok();
    }
};

}  // namespace kernel::arch::x86_64::core_init

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_CORE_INIT_HPP_
