#ifndef DOOM_OS_KERNEL_DEVICE_LIFECYCLE_HPP_
#define DOOM_OS_KERNEL_DEVICE_LIFECYCLE_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/init/component.hpp"

#include <uk/registry.hpp>

namespace uk {

// =================================================================================================
// Driver lifecycle
// =================================================================================================

init_result init_all();
void exit_all();
void init_all_or_halt();

struct component : kernel::init::component<component, kernel::init::no_resource> {
    static constexpr auto *name = "DRIVERS";

    template <typename View>
    static kernel::init::init_result init(View)
    {
        init_result outcome = init_all();

        if (outcome.is_err()) {
            return kernel::core::Err(kernel::init::init_error::UNSPECIFIED);
        }

        return kernel::core::Ok();
    }
};

}  // namespace uk

#endif  // DOOM_OS_KERNEL_DEVICE_LIFECYCLE_HPP_
