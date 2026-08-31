#ifndef DOOM_OS_KERNEL_BOOT_BOOT_PLAN_HPP_
#define DOOM_OS_KERNEL_BOOT_BOOT_PLAN_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/core_init.hpp"
#include "kernel/arch/x86_64/serial/serial.hpp"
#include "kernel/boot/boot_info.hpp"
#include "kernel/core/component.hpp"
#include "kernel/core/memory/pmm/pmm.hpp"
#include "kernel/core/memory/vmm/mmu/mmu.hpp"
#include "kernel/runtime/init.hpp"

namespace kernel::boot {
// =================================================================================================
// Early boot
// =================================================================================================

using early_boot_roots = kernel::core::type_list<kernel::arch::x86_64::serial::component>;

// =================================================================================================
// Platform boot
//
// Brings up fault handling. Nothing here touches bootloader-supplied data, because until the
// IDT is live an exception is a triple fault: silent reset, no panic, no register dump.
// =================================================================================================

using platform_roots = kernel::core::type_list<kernel::arch::x86_64::core_init::component>;

// =================================================================================================
// Main boot
//
// Subsystems that do real work on bootloader-supplied data. Ordered after platform_roots so a
// fault in any of them is reportable. The phases are separate graphs rather than roots of one
// graph because their relative order would otherwise come from position in a type_list, which
// is the kind of ordering nobody writes down and everybody eventually breaks.
//
// Rooted on the C++ runtime rather than on the MMU, because the runtime depends on it: the sort
// pulls in mmu -> pmm -> boot_info underneath and puts global construction last, where it has a
// heap to allocate from. Stating it as an edge rather than as a second call after this graph is
// the difference between an ordering the compiler checks and one a comment asks for.
// =================================================================================================

using boot_roots = kernel::core::type_list<kernel::runtime::component>;

}  // namespace kernel::boot

#endif  // DOOM_OS_KERNEL_BOOT_BOOT_PLAN_HPP_
