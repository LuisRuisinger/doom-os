#ifndef DOOM_OS_KERNEL_ARCH_X86_64_IDT_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_IDT_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/gdt.hpp"
#include "kernel/boot/component.hpp"
#include "kernel/core/types.hpp"

namespace kernel::arch::x86_64::idt {

using kernel::core::u8;
using kernel::core::u16;
using kernel::core::u64;

// =================================================================================================
// IDT constants
// =================================================================================================

static constexpr u16 ENTRY_COUNT = 256;

static constexpr u8 GATE_TYPE_INTERRUPT = 0x8E;
static constexpr u8 GATE_TYPE_TRAP      = 0x8F;
static constexpr u8 GATE_TYPE_USER_TRAP = 0xEF;

// =================================================================================================
// Public API
// =================================================================================================

using handler = void (*)();

void init();

void set_interrupt_gate(u8 vector, handler handler_address);
void set_trap_gate(u8 vector, handler handler_address);
void set_user_trap_gate(u8 vector, handler handler_address);

// =================================================================================================
// Boot component
// =================================================================================================

struct component : kernel::boot::component_base<
    component,
    kernel::boot::type_list<kernel::arch::x86_64::gdt::component>
> {
    static constexpr const char* name = "IDT";

    static bool init_component() {
        init();
        return true;
    }
};

} // namespace kernel::arch::x86_64::idt

#endif // DOOM_OS_KERNEL_ARCH_X86_64_IDT_HPP_