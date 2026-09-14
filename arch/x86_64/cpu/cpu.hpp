#ifndef DOOM_OS_KERNEL_ARCH_X86_64_CPU_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_CPU_HPP_

#include "arch/x86_64/cpu/stacks.hpp"
#include "arch/x86_64/gdt/gdt.hpp"
#include "arch/x86_64/idt/idt.hpp"
#include "arch/x86_64/tss/tss.hpp"
#include "kernel/core/types.hpp"
#include "kernel/init/component.hpp"

namespace kernel::arch::x86_64::cpu {

using kernel::core::u16;
using kernel::core::u32;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

static constexpr u32 MAX_CORE_COUNT = 64;

class local_state;

void init_bsp();

class local_state {
    u32  logical_id_m{};
    u32  apic_id_m{};
    bool is_bsp_m{};
    bool is_online_m{};

    stack_set       stacks_m{};
    tss::state      tss_m{};
    gdt::table      gdt_m{};
    idt::gate_table exception_gates_m{};
    idt::table      idt_m{};

    template <typename, typename>
    friend struct kernel::init::resource_binding;

    friend void init_bsp();

public:
    [[nodiscard]] u32 logical_id() const
    {
        return logical_id_m;
    }

    [[nodiscard]] u32 apic_id() const
    {
        return apic_id_m;
    }

    [[nodiscard]] bool is_bsp() const
    {
        return is_bsp_m;
    }

    [[nodiscard]] bool is_online() const
    {
        return is_online_m;
    }

    [[nodiscard]] const stack_set &stacks() const
    {
        return stacks_m;
    }
};

local_state &bsp();
local_state &current();
local_state *get(u32 logical_id);
u32 online_count();

inline void relax()
{
    asm volatile("pause");
}

[[noreturn]] inline void halt()
{
    for (;;)
        asm volatile("cli; hlt");
}

struct component : kernel::init::component<component, kernel::init::no_resource> {
    static constexpr auto *name = "CPU";

    template <typename View>
    static kernel::init::init_result init(View)
    {
        init_bsp();
        return kernel::core::Ok();
    }
};

}  // namespace kernel::arch::x86_64::cpu

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_CPU_HPP_
