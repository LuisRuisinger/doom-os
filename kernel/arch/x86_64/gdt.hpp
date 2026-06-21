#ifndef DOOM_OS_KERNEL_ARCH_X86_64_GDT_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_GDT_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/component.hpp"
#include "kernel/core/types.hpp"

namespace kernel::arch::x86_64::gdt {

using kernel::core::u16;

// =================================================================================================
// Segment selectors
// =================================================================================================

static constexpr u16 NULL_SELECTOR        = 0x00;
static constexpr u16 KERNEL_CODE_SELECTOR = 0x08;
static constexpr u16 KERNEL_DATA_SELECTOR = 0x10;
static constexpr u16 USER_DATA_SELECTOR   = 0x18;
static constexpr u16 USER_CODE_SELECTOR   = 0x20;

// =================================================================================================
// Public API
// =================================================================================================

void init();

// =================================================================================================
// Boot component
// =================================================================================================

struct component : kernel::boot::component_base<component> {
    static constexpr const char* name = "GDT";

    static bool init_component() {
        init();
        return true;
    }
};

} // namespace kernel::arch::x86_64::gdt

#endif // DOOM_OS_KERNEL_ARCH_X86_64_GDT_HPP_