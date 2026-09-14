#ifndef DOOM_OS_KERNEL_ARCH_X86_64_IDT_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_IDT_HPP_

#include "arch/x86_64/gdt/gdt.hpp"
#include "kernel/core/types.hpp"
#include "kernel/init/component.hpp"

namespace kernel::arch::x86_64::exceptions {

struct core_component;

}  // namespace kernel::arch::x86_64::exceptions

namespace kernel::arch::x86_64::idt {

using kernel::core::u16;
using kernel::core::u32;
using kernel::core::u64;
using kernel::core::u8;

static constexpr u16 ENTRY_COUNT = 256;

static constexpr u8 GATE_TYPE_INTERRUPT = 0x8E;
static constexpr u8 GATE_TYPE_TRAP = 0x8F;
static constexpr u8 GATE_TYPE_USER_TRAP = 0xEF;

using handler = void (*)();

struct gate_spec {
    handler entry_point;
    u8      type_attributes;
    u8      ist;
};

class gate_table {
    gate_spec gates_m[ENTRY_COUNT]{};

public:
    void set_interrupt_gate(u8 vector, handler entry_point, u8 ist = 0);
    void set_trap_gate(u8 vector, handler entry_point, u8 ist = 0);
    void set_user_trap_gate(u8 vector, handler entry_point, u8 ist = 0);

    [[nodiscard]] const gate_spec &operator[](u16 vector) const
    {
        return gates_m[vector];
    }
};

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
    alignas(16) entry entries_m[ENTRY_COUNT]{};
    pointer ptr_m{};

public:
    void init(const gate_table &gates, u16 code_selector);
    void load() const;
};

struct core_component
    : kernel::init::component<core_component, table, kernel::arch::x86_64::gdt::core_component,
                              kernel::arch::x86_64::exceptions::core_component> {
    static constexpr auto *name = "IDT";

    template <typename View>
    static kernel::init::init_result init(View view)
    {
        table &self = own(view);

        self.init(dep<kernel::arch::x86_64::exceptions::core_component>(view),
                  dep<kernel::arch::x86_64::gdt::core_component>(view).kernel_code_selector());
        self.load();

        return kernel::core::Ok();
    }
};

}  // namespace kernel::arch::x86_64::idt

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_IDT_HPP_
