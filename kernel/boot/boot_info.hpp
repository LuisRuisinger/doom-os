#ifndef DOOM_OS_KERNEL_BOOT_BOOT_INFO_HPP_
#define DOOM_OS_KERNEL_BOOT_BOOT_INFO_HPP_

#include "kernel/boot/protocol.hpp"
#include "kernel/init/component.hpp"

namespace kernel::boot::boot_info {

using kernel::core::paddr_t;
using kernel::core::u64;

void set_handoff(kernel::boot::handoff source);
void set_handoff(u64 magic, paddr_t address);

const kernel::boot::handoff &current_handoff();

const kernel::boot::info &current();

bool available();

struct component : kernel::init::component<component, kernel::init::no_resource> {
    static constexpr auto *name = "BOOT_INFO";

    static kernel::init::init_result parse();

    template <typename View>
    static kernel::init::init_result init(View)
    {
        return parse();
    }
};

}  // namespace kernel::boot::boot_info

#endif  // DOOM_OS_KERNEL_BOOT_BOOT_INFO_HPP_
