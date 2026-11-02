#ifndef DOOM_OS_KERNEL_BOOT_BOOT_INFO_HPP_
#define DOOM_OS_KERNEL_BOOT_BOOT_INFO_HPP_

#include "kernel/boot/protocol.hpp"
#include "kernel/init/component.hpp"

namespace kernel::boot::boot_info {

const kernel::boot::info &current();

struct component : kernel::init::component<component, kernel::init::no_resource> {
    static constexpr const char *name = "BOOT_INFO";

    template <typename View>
    static kernel::init::init_result init(View /*unused*/)
    {
        return parse();
    }

private:
    static kernel::init::init_result parse();
};

}  // namespace kernel::boot::boot_info

#endif  // DOOM_OS_KERNEL_BOOT_BOOT_INFO_HPP_
