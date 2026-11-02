#ifndef DOOM_OS_PLATFORM_PC_BOOT_PLAN_HPP_
#define DOOM_OS_PLATFORM_PC_BOOT_PLAN_HPP_

#include "arch/x86_64/core_init.hpp"
#include "arch/x86_64/lapic/lapic.hpp"
#include "kernel/init/component.hpp"
#include "kernel/runtime/init.hpp"
#include "platform/pc/early_console.hpp"

namespace kernel::platform {

using early_boot_roots = kernel::init::type_list<pc::early_console::component>;

using platform_roots = kernel::init::type_list<kernel::arch::x86_64::core_init::component>;

using boot_roots =
    kernel::init::type_list<kernel::arch::x86_64::lapic::component, kernel::runtime::component>;

}  // namespace kernel::platform

#endif  // DOOM_OS_PLATFORM_PC_BOOT_PLAN_HPP_
