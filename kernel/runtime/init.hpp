#ifndef DOOM_OS_KERNEL_RUNTIME_INIT_HPP_
#define DOOM_OS_KERNEL_RUNTIME_INIT_HPP_

#include "kernel/init/component.hpp"
#include "kernel/mm/kheap.hpp"

namespace kernel::runtime {

void call_global_constructors();
void call_global_destructors();

struct component
    : kernel::init::component<component, kernel::init::no_resource, kernel::mm::kheap::component> {
    static constexpr auto *name = "RUNTIME";

    template <typename View>
    static kernel::init::init_result init(View)
    {
        call_global_constructors();

        return kernel::core::Ok();
    }
};

}  // namespace kernel::runtime

#endif  // DOOM_OS_KERNEL_RUNTIME_INIT_HPP_
