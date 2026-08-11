#ifndef DOOM_OS_KERNEL_BOOT_BOOT_INFO_HPP_
#define DOOM_OS_KERNEL_BOOT_BOOT_INFO_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/component.hpp"
#include "kernel/boot/protocol.hpp"

namespace kernel::boot::boot_info {

using kernel::core::paddr_t;
using kernel::core::u64;

// =================================================================================================
// Handoff
//
// Recorded by the entry point before any graph runs, because it only exists in the registers
// the bootloader jumped in with.
// =================================================================================================

void set_handoff(kernel::boot::handoff source);
void set_handoff(u64 magic, paddr_t address);

const kernel::boot::handoff &current_handoff();

// =================================================================================================
// Boot description
// =================================================================================================

const kernel::boot::info &current();

bool available();

// =================================================================================================
// Component
// =================================================================================================

struct component : kernel::core::component<component, kernel::core::no_resource> {
    static constexpr auto *name = "BOOT_INFO";

    static kernel::core::init_result parse();

    template <typename View>
    static kernel::core::init_result init(View)
    {
        return parse();
    }
};

}  // namespace kernel::boot::boot_info

#endif  // DOOM_OS_KERNEL_BOOT_BOOT_INFO_HPP_
