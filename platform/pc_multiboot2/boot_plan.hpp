#ifndef DOOM_OS_PLATFORM_PC_MULTIBOOT2_BOOT_PLAN_HPP_
#define DOOM_OS_PLATFORM_PC_MULTIBOOT2_BOOT_PLAN_HPP_

#include "arch/x86_64/core_init.hpp"
#include "arch/x86_64/lapic/lapic.hpp"
#include "arch/x86_64/mmu/mmu.hpp"
#include "kernel/boot/boot_info.hpp"
#include "kernel/init/component.hpp"
#include "kernel/mm/kheap.hpp"
#include "kernel/mm/pmm.hpp"
#include "kernel/runtime/init.hpp"
#include "platform/pc_multiboot2/early_console.hpp"

namespace kernel::platform::pc_multiboot2 {

using early_boot_roots = kernel::init::type_list<early_console::component>;

using platform_roots = kernel::init::type_list<kernel::arch::x86_64::core_init::component>;

using boot_roots =
    kernel::init::type_list<kernel::arch::x86_64::lapic::component, kernel::runtime::component>;

}  // namespace kernel::platform::pc_multiboot2

#endif  // DOOM_OS_PLATFORM_PC_MULTIBOOT2_BOOT_PLAN_HPP_
