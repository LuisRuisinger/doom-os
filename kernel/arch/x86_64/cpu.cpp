// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/cpu.hpp"

namespace kernel::arch::x86_64::cpu {
// =================================================================================================
// Storage
// =================================================================================================

static local_state bsp_state{};
static u32         current_online_count = 0;

// =================================================================================================
// Local CPU state
// =================================================================================================

u64 local_state::kernel_stack_top() const {
    return reinterpret_cast<u64>(&kernel_stack[CORE_STACK_SIZE]);
}

u64 local_state::double_fault_stack_top() const {
    return reinterpret_cast<u64>(&double_fault_stack[CORE_STACK_SIZE]);
}

// =================================================================================================
// BSP
// =================================================================================================

void init_bsp() {
    bsp_state.logical_id = 0;
    bsp_state.apic_id = 0;
    bsp_state.is_bsp = true;
    bsp_state.is_online = true;

    current_online_count = 1;
}

local_state &bsp() { return bsp_state; }

// =================================================================================================
// CPU state storage
// =================================================================================================

local_state *get(u32 logical_id) {
    if (logical_id == 0) {
        return &bsp_state;
    }

    return nullptr;
}

u32 online_count() { return current_online_count; }
}  // namespace kernel::arch::x86_64::cpu