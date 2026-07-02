#ifndef DOOM_OS_KERNEL_ARCH_X86_64_GDT_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_GDT_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "../tss/tss.hpp"
#include "kernel/boot/component.hpp"
#include "kernel/core/types.hpp"

namespace kernel::arch::x86_64::cpu {

struct local_state;

}  // namespace kernel::arch::x86_64::cpu

namespace kernel::arch::x86_64::gdt {

using kernel::core::u16;
using kernel::core::u64;
using kernel::core::u8;

// =================================================================================================
// Selectors
// =================================================================================================

static constexpr u16 NULL_SELECTOR = 0x00;
static constexpr u16 KERNEL_CODE_SELECTOR = 0x08;
static constexpr u16 KERNEL_DATA_SELECTOR = 0x10;
static constexpr u16 USER_DATA_SELECTOR = 0x18;
static constexpr u16 USER_CODE_SELECTOR = 0x20;
static constexpr u16 TSS_SELECTOR = 0x28;

// =================================================================================================
// Table
// =================================================================================================

static constexpr u16 ENTRY_COUNT = 7;

struct [[gnu::packed]] descriptor {
    u16 limit_low;
    u16 base_low;
    u8  base_mid;
    u8  access;
    u8  limit_high_flags;
    u8  base_high;
};

struct [[gnu::packed]] pointer {
    u16 limit;
    u64 base;
};

class table {
    alignas(8) descriptor entries_m[ENTRY_COUNT]{};
    pointer ptr_m{};

   public:
    void init(const kernel::arch::x86_64::tss::state &task_state_segment);
    void load() const;
};

// =================================================================================================
// GDT
// =================================================================================================

void init_table(table &table, const kernel::arch::x86_64::tss::state &task_state_segment);
void load_table(const table &table);
void load_task_register(u16 selector);

// =================================================================================================
// Core component
// =================================================================================================

struct core_component : kernel::boot::context_component_base<
                            core_component, kernel::arch::x86_64::cpu::local_state,
                            kernel::boot::type_list<kernel::arch::x86_64::tss::core_component>> {
    static constexpr auto *name = "GDT";

    static bool init_component(kernel::arch::x86_64::cpu::local_state &cpu);
};

}  // namespace kernel::arch::x86_64::gdt

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_GDT_HPP_
