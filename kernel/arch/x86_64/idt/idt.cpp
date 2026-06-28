// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/idt/idt.hpp"

#include "../cpu/cpu.hpp"
#include "../gdt/gdt.hpp"

namespace kernel::arch::x86_64::idt {
using kernel::core::u16;
using kernel::core::u32;
using kernel::core::u64;
using kernel::core::u8;

// =================================================================================================
// Assembly
// =================================================================================================

extern "C" void idt_load(const void *idt_pointer);

// =================================================================================================
// Static checks
// =================================================================================================

static_assert(sizeof(entry) == 16);
static_assert(sizeof(pointer) == 10);

// =================================================================================================
// Table
// =================================================================================================

void table::init() {
    for (u16 i = 0; i < ENTRY_COUNT; ++i) {
        entries_[i] = entry{
            .offset_low = 0,
            .selector = 0,
            .ist = 0,
            .type_attributes = 0,
            .offset_mid = 0,
            .offset_high = 0,
            .reserved = 0,
        };
    }

    pointer_ = pointer{
        .limit = static_cast<u16>(sizeof(entries_) - 1),
        .base = reinterpret_cast<u64>(&entries_[0]),
    };
}

void table::load() const { idt_load(&pointer_); }

void table::set_interrupt_gate(u8 vector, handler handler_address, u8 ist) {
    set_gate(vector, handler_address, GATE_TYPE_INTERRUPT, ist);
}

void table::set_trap_gate(u8 vector, handler handler_address, u8 ist) {
    set_gate(vector, handler_address, GATE_TYPE_TRAP, ist);
}

void table::set_user_trap_gate(u8 vector, handler handler_address, u8 ist) {
    set_gate(vector, handler_address, GATE_TYPE_USER_TRAP, ist);
}

void table::set_gate(u8 vector, handler handler_address, u8 type_attributes, u8 ist) {
    const auto address = reinterpret_cast<u64>(handler_address);

    entries_[vector] = entry{
        .offset_low = static_cast<u16>(address & 0xFFFF),
        .selector = kernel::arch::x86_64::gdt::KERNEL_CODE_SELECTOR,
        .ist = static_cast<u8>(ist & 0x07),
        .type_attributes = type_attributes,
        .offset_mid = static_cast<u16>((address >> 16) & 0xFFFF),
        .offset_high = static_cast<u32>((address >> 32) & 0xFFFFFFFF),
        .reserved = 0,
    };
}

// =================================================================================================
// IDT
// =================================================================================================

void init_table(table &target) { target.init(); }

void load_table(const table &target) { target.load(); }

// =================================================================================================
// Core component
// =================================================================================================

bool core_component::init_component(kernel::arch::x86_64::cpu::local_state &cpu) {
    init_table(cpu.idt);

    /*
     * Do not load the IDT yet.
     *
     * The table is still empty until the exceptions core component installs
     * exception gates.
     */
    return true;
}
}  // namespace kernel::arch::x86_64::idt