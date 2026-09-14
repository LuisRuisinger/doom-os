
#include "arch/x86_64/cpu/cpu.hpp"

#include "kernel/sync/atomic.hpp"

namespace kernel::arch::x86_64::cpu {

static local_state               bsp_state{};
static kernel::sync::atomic<u32> current_online_count{};

void init_bsp()
{
    bsp_state.logical_id_m = 0;
    bsp_state.apic_id_m = 0;
    bsp_state.is_bsp_m = true;
    bsp_state.is_online_m = true;

    current_online_count.fetch_add(1);
}

local_state &bsp()
{
    return bsp_state;
}

local_state &current()
{
    return bsp();
}

local_state *get(u32 logical_id)
{
    return logical_id == 0 ? &bsp_state : nullptr;
}

u32 online_count()
{
    return current_online_count.load(sync::memory_order::ACQUIRE);
}

}  // namespace kernel::arch::x86_64::cpu
