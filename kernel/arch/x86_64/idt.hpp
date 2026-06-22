#ifndef DOOM_OS_KERNEL_ARCH_X86_64_IDT_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_IDT_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/component.hpp"
#include "kernel/core/types.hpp"

namespace kernel::arch::x86_64::cpu {
struct local_state;
}  // namespace kernel::arch::x86_64::cpu

namespace kernel::arch::x86_64::gdt {
struct core_component;
}  // namespace kernel::arch::x86_64::gdt

namespace kernel::arch::x86_64::idt {
using kernel::core::u16;
using kernel::core::u32;
using kernel::core::u64;
using kernel::core::u8;

// =================================================================================================
// Constants
// =================================================================================================

static constexpr u16 ENTRY_COUNT = 256;

static constexpr u8 GATE_TYPE_INTERRUPT = 0x8E;
static constexpr u8 GATE_TYPE_TRAP = 0x8F;
static constexpr u8 GATE_TYPE_USER_TRAP = 0xEF;

using handler = void (*)();

// =================================================================================================
// Table
// =================================================================================================

struct [[gnu::packed]] entry {
    u16 offset_low;
    u16 selector;
    u8  ist;
    u8  type_attributes;
    u16 offset_mid;
    u32 offset_high;
    u32 reserved;
};

struct [[gnu::packed]] pointer {
    u16 limit;
    u64 base;
};

class table {
   public:
    void init();

    void load() const;

    void set_interrupt_gate(u8 vector, handler handler_address, u8 ist = 0);

    void set_trap_gate(u8 vector, handler handler_address, u8 ist = 0);

    void set_user_trap_gate(u8 vector, handler handler_address, u8 ist = 0);

   private:
    void set_gate(u8 vector, handler handler_address, u8 type_attributes, u8 ist);

    alignas(16) entry entries_[ENTRY_COUNT]{};
    pointer pointer_{};
};

// =================================================================================================
// IDT
// =================================================================================================

void init_table(table &table);

void load_table(const table &table);

// =================================================================================================
// Core component
// =================================================================================================

struct core_component : kernel::boot::context_component_base<
                            core_component, kernel::arch::x86_64::cpu::local_state,
                            kernel::boot::type_list<kernel::arch::x86_64::gdt::core_component> > {
    static constexpr const char *name = "IDT";

    static bool init_component(kernel::arch::x86_64::cpu::local_state &cpu);
};
}  // namespace kernel::arch::x86_64::idt

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_IDT_HPP_