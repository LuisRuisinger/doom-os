// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/idt/idt.hpp"

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
// Gate table
// =================================================================================================

void gate_table::set_interrupt_gate(u8 vector, handler entry_point, u8 ist)
{
    gates_m[vector] = gate_spec{entry_point, GATE_TYPE_INTERRUPT, ist};
}

void gate_table::set_trap_gate(u8 vector, handler entry_point, u8 ist)
{
    gates_m[vector] = gate_spec{entry_point, GATE_TYPE_TRAP, ist};
}

void gate_table::set_user_trap_gate(u8 vector, handler entry_point, u8 ist)
{
    gates_m[vector] = gate_spec{entry_point, GATE_TYPE_USER_TRAP, ist};
}

// =================================================================================================
// Table
// =================================================================================================

static entry make_entry(const gate_spec &spec, u16 code_selector)
{
    if (spec.entry_point == nullptr) {
        return entry{};
    }

    const auto address = reinterpret_cast<u64>(spec.entry_point);

    return entry{
        .offset_low = static_cast<u16>(address & 0xFFFF),
        .selector = code_selector,
        .ist = static_cast<u8>(spec.ist & 0x07),
        .type_attributes = spec.type_attributes,
        .offset_mid = static_cast<u16>((address >> 16) & 0xFFFF),
        .offset_high = static_cast<u32>((address >> 32) & 0xFFFFFFFF),
        .reserved = 0,
    };
}

void table::init(const gate_table &gates, u16 code_selector)
{
    for (u16 i = 0; i < ENTRY_COUNT; ++i) {
        entries_m[i] = make_entry(gates[i], code_selector);
    }

    ptr_m = pointer{
        .limit = static_cast<u16>(sizeof(entries_m) - 1),
        .base = reinterpret_cast<u64>(&entries_m[0]),
    };
}

void table::load() const
{
    idt_load(&ptr_m);
}

}  // namespace kernel::arch::x86_64::idt
