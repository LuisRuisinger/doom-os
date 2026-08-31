#ifndef DOOM_OS_KERNEL_RUNTIME_INIT_HPP_
#define DOOM_OS_KERNEL_RUNTIME_INIT_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/component.hpp"
#include "kernel/core/memory/vmm/mmu/mmu.hpp"

namespace kernel::runtime {

void call_global_constructors();
void call_global_destructors();

struct component
    : kernel::core::component<component, kernel::core::no_resource,
                              kernel::core::memory::vmm::mmu::component> {
    static constexpr auto *name = "CGC";

    template <typename View>
    static kernel::core::init_result init(View)
    {
        call_global_constructors();

        return kernel::core::Ok();
    }
};

}  // namespace kernel::runtime

#endif  // DOOM_OS_KERNEL_RUNTIME_INIT_HPP_
