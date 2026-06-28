// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/tss/tss.hpp"

#include "kernel/arch/x86_64/cpu/cpu.hpp"

namespace kernel::arch::x86_64::tss {

// =================================================================================================
// Initialization
// =================================================================================================

void state::clear() {
    layout_m.reserved0 = 0;

    for (u8 i = 0; i < PRIVILEGE_STACK_COUNT; ++i) {
        layout_m.rsp[i] = 0;
    }

    layout_m.reserved1 = 0;

    for (u8 i = 0; i < INTERRUPT_STACK_COUNT; ++i) {
        layout_m.ist[i] = 0;
    }

    layout_m.reserved2 = 0;
    layout_m.reserved3 = 0;
    layout_m.io_map_base = IO_MAP_DISABLED_BASE;
}

bool state::set_privilege_stack(u8 privilege_level, u64 stack_top) {
    if (privilege_level >= PRIVILEGE_STACK_COUNT) {
        return false;
    }

    layout_m.rsp[privilege_level] = stack_top;
    return true;
}

bool state::set_interrupt_stack(interrupt_stack stack, u64 stack_top) {
    const u8 stack_index = static_cast<u8>(stack);

    if (stack_index == NO_INTERRUPT_STACK || stack_index > INTERRUPT_STACK_COUNT) {
        return false;
    }

    layout_m.ist[stack_index - 1] = stack_top;
    return true;
}

void state::init(const stack_config &config) {
    clear();

    layout_m.rsp[0] = config.rsp0;
    layout_m.rsp[1] = config.rsp1;
    layout_m.rsp[2] = config.rsp2;

    layout_m.ist[0] = config.ist1;
    layout_m.ist[1] = config.ist2;
    layout_m.ist[2] = config.ist3;
    layout_m.ist[3] = config.ist4;
    layout_m.ist[4] = config.ist5;
    layout_m.ist[5] = config.ist6;
    layout_m.ist[6] = config.ist7;
}

const segment &state::layout() const { return layout_m; }

u64 state::base() const { return reinterpret_cast<u64>(&layout_m); }

// =================================================================================================
// Core component
// =================================================================================================

bool core_component::init_component(kernel::arch::x86_64::cpu::local_state &cpu) {
    cpu.init_task_state_segment();
    return true;
}

}  // namespace kernel::arch::x86_64::tss
