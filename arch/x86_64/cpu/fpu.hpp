#ifndef DOOM_OS_KERNEL_ARCH_X86_64_CPU_FPU_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_CPU_FPU_HPP_

#include "kernel/init/component.hpp"

namespace kernel::arch::x86_64::cpu {

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
