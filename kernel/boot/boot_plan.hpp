#ifndef DOOM_OS_KERNEL_BOOT_BOOT_PLAN_HPP_
#define DOOM_OS_KERNEL_BOOT_BOOT_PLAN_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/serial.hpp"
#include "kernel/arch/x86_64/gdt.hpp"
#include "kernel/arch/x86_64/idt.hpp"

namespace kernel::boot {

// =================================================================================================
// Boot roots
// =================================================================================================

using early_boot_roots = type_list<
    kernel::arch::x86_64::serial::component
>;

using boot_roots = type_list<
    kernel::arch::x86_64::gdt::component,
    kernel::arch::x86_64::idt::component
>;

} // namespace kernel::boot

#endif // DOOM_OS_KERNEL_BOOT_BOOT_PLAN_HPP_