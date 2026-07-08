#ifndef DOOM_OS_KERNEL_BOOT_BOOT_PLAN_HPP_
#define DOOM_OS_KERNEL_BOOT_BOOT_PLAN_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/core_init.hpp"
#include "kernel/arch/x86_64/serial/serial.hpp"
#include "kernel/boot/boot_info.hpp"
#include "kernel/boot/component.hpp"

namespace kernel::boot {
// =================================================================================================
// Early boot
// =================================================================================================

using early_boot_roots = type_list<kernel::arch::x86_64::serial::component>;

// =================================================================================================
// Main boot
// =================================================================================================

using boot_roots =
    type_list<kernel::boot::boot_info::component, kernel::arch::x86_64::core_init::component>;

}  // namespace kernel::boot

#endif  // DOOM_OS_KERNEL_BOOT_BOOT_PLAN_HPP_
