#ifndef DOOM_OS_PLATFORM_PC_MULTIBOOT2_BOOT_PLAN_HPP_
#define DOOM_OS_PLATFORM_PC_MULTIBOOT2_BOOT_PLAN_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

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
// =================================================================================================
// Early boot
// =================================================================================================

using early_boot_roots = kernel::init::type_list<early_console::component>;

// =================================================================================================
// Platform boot
//
// Brings up fault handling. Nothing here touches bootloader-supplied data, because until the
// IDT is live an exception is a triple fault: silent reset, no panic, no register dump.
// =================================================================================================

using platform_roots = kernel::init::type_list<kernel::arch::x86_64::core_init::component>;

// =================================================================================================
// Main boot
//
// Subsystems that do real work on bootloader-supplied data. Ordered after platform_roots so a
// fault in any of them is reportable. The phases are separate graphs rather than roots of one
// graph because their relative order would otherwise come from position in a type_list, which
// is the kind of ordering nobody writes down and everybody eventually breaks.
//
// LAPIC is the PC-specific consumer of the generic ACPI table transport. Its dependency pulls in
// boot_info -> pmm -> mmu -> vmm -> acpi before the MADT is parsed. The C++ runtime remains a root
// because global constructors are entitled to allocate, and the heap pulls its own dependencies in
// through the same checked graph.
// =================================================================================================

using boot_roots =
    kernel::init::type_list<kernel::arch::x86_64::lapic::component, kernel::runtime::component>;

}  // namespace kernel::platform::pc_multiboot2

#endif  // DOOM_OS_PLATFORM_PC_MULTIBOOT2_BOOT_PLAN_HPP_
