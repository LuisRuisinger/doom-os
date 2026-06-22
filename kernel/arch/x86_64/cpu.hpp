#ifndef DOOM_OS_KERNEL_ARCH_X86_64_CPU_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_CPU_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/gdt.hpp"
#include "kernel/arch/x86_64/idt.hpp"
#include "kernel/boot/component.hpp"
#include "kernel/core/types.hpp"

namespace kernel::arch::x86_64::cpu {

using kernel::core::u32;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

// =================================================================================================
// Constants
// =================================================================================================

static constexpr usize CORE_STACK_SIZE = 16 * 1024;
static constexpr u32   MAX_CORE_COUNT = 64;

// =================================================================================================
// Local CPU state
// =================================================================================================

struct local_state {
    u32  logical_id;
    u32  apic_id;
    bool is_bsp;
    bool is_online;

    gdt::table gdt;
    idt::table idt;

    alignas(16) u8 kernel_stack[CORE_STACK_SIZE];
    alignas(16) u8 double_fault_stack[CORE_STACK_SIZE];

    u64 kernel_stack_top() const;

    u64 double_fault_stack_top() const;
};

// =================================================================================================
// CPU state storage
// =================================================================================================

void init_bsp();

local_state &bsp();

local_state *get(u32 logical_id);

u32 online_count();

// =================================================================================================
// Component
// =================================================================================================

struct component : kernel::boot::component_base<component> {
    static constexpr const char *name = "CPU";

    static bool init_component() {
        init_bsp();
        return true;
    }
};
}  // namespace kernel::arch::x86_64::cpu

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_CPU_HPP_