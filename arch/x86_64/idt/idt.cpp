
#include "arch/x86_64/idt/idt.hpp"

#include "kernel/core/cast.hpp"

namespace kernel::arch::x86_64::idt {

using kernel::core::u16;
using kernel::core::u32;
using kernel::core::u64;
using kernel::core::u8;

extern "C" void idt_load(const void *idt_pointer);

static_assert(sizeof(entry) == 16);
static_assert(sizeof(pointer) == 10);

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

static entry make_entry(const gate_spec &spec, u16 code_selector)
{
    if (spec.entry_point == nullptr) {
        return entry{};
    }

    const auto address = spec.entry_point as(u64);

    return entry{
        .offset_low = (address & 0xFFFF) as(u16),
        .selector = code_selector,
        .ist = (spec.ist & 0x07) as(u8),
        .type_attributes = spec.type_attributes,
        .offset_mid = ((address >> 16) & 0xFFFF) as(u16),
        .offset_high = ((address >> 32) & 0xFFFFFFFF) as(u32),
        .reserved = 0,
    };
}

void table::init(const gate_table &gates, u16 code_selector)
{
    for (u16 i = 0; i < ENTRY_COUNT; ++i) {
        entries_m[i] = make_entry(gates[i], code_selector);
    }

    ptr_m = pointer{
        .limit = (sizeof(entries_m) - 1) as(u16),
        .base = (&entries_m[0]) as(u64),
    };
}

void table::load() const
{
    idt_load(&ptr_m);
}

}  // namespace kernel::arch::x86_64::idt
