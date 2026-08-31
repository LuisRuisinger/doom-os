#ifndef DOOM_OS_KERNEL_DRIVER_LIFECYCLE_HPP_
#define DOOM_OS_KERNEL_DRIVER_LIFECYCLE_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/component.hpp"

#include <kernel/driver/registry.hpp>

namespace kernel::driver {

// =================================================================================================
// Driver lifecycle
// =================================================================================================

init_result init_all();
void exit_all();
void init_all_or_halt();

struct component : kernel::core::component<component, kernel::core::no_resource> {
    static constexpr auto *name = "DRIVERS";

    template <typename View>
    static kernel::core::init_result init(View)
    {
        init_result outcome = init_all();

        if (outcome.is_err()) {
            return kernel::core::Err(kernel::core::init_error::UNSPECIFIED);
        }

        return kernel::core::Ok();
    }
};

}  // namespace kernel::driver

#endif  // DOOM_OS_KERNEL_DRIVER_LIFECYCLE_HPP_
