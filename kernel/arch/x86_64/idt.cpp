// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/idt.hpp"

#include "kernel/arch/x86_64/gdt.hpp"
#include "kernel/core/types.hpp"

namespace kernel::arch::x86_64::idt {

using kernel::core::u8;
using kernel::core::u16;
using kernel::core::u32;
using kernel::core::u64;

// =================================================================================================
// External assembly
// =================================================================================================

extern "C" void idt_load(const void* idt_pointer);

// =================================================================================================
// IDT structures
// =================================================================================================

struct [[gnu::packed]] entry {
    u16 offset_low;
    u16 selector;
    u8 ist;
    u8 type_attributes;
    u16 offset_mid;
    u32 offset_high;
    u32 reserved;
};

struct [[gnu::packed]] pointer {
    u16 limit;
    u64 base;
};

static_assert(sizeof(entry) == 16);
static_assert(sizeof(pointer) == 10);

// =================================================================================================
// IDT storage
// =================================================================================================

alignas(16) static entry idt_entries[ENTRY_COUNT]{};

static pointer idt_pointer{
    .limit = static_cast<u16>(sizeof(idt_entries) - 1),
    .base = reinterpret_cast<u64>(&idt_entries[0]),
};

// =================================================================================================
// IDT entry construction
// =================================================================================================

static void set_gate(u8 vector, handler handler_address, u8 type_attributes, u8 ist = 0) {
    const auto address = reinterpret_cast<u64>(handler_address);

    idt_entries[vector] = entry{
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
// Public API
// =================================================================================================

void init() {
    idt_entries[0] = {};
    idt_load(&idt_pointer);
}

void set_interrupt_gate(u8 vector, handler handler_address) {
    set_gate(vector, handler_address, GATE_TYPE_INTERRUPT);
}

void set_trap_gate(u8 vector, handler handler_address) {
    set_gate(vector, handler_address, GATE_TYPE_TRAP);
}

void set_user_trap_gate(u8 vector, handler handler_address) {
    set_gate(vector, handler_address, GATE_TYPE_USER_TRAP);
}

} // namespace kernel::arch::x86_64::idt