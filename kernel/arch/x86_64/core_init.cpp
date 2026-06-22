// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/core_init.hpp"

#include "kernel/arch/x86_64/core_plan.hpp"
#include "kernel/boot/init.hpp"
#include "kernel/debug/kprint.hpp"

namespace kernel::arch::x86_64::core_init {
// =================================================================================================
// Logging
// =================================================================================================

struct core_logger {
    template <typename Component>
    void begin(cpu::local_state &cpu) {
        KPRINTLN("[init][core {}] {}", cpu.logical_id, Component::name);
    }

    template <typename Component>
    void ok(cpu::local_state &cpu) {
        KPRINTLN("[init][core {}] {}: OK", cpu.logical_id, Component::name);
    }

    template <typename Component>
    void fail(cpu::local_state &cpu) {
        KPRINTLN("[init][core {}] {}: FAIL", cpu.logical_id, Component::name);
    }
};

// =================================================================================================
// Core initialization
// =================================================================================================

bool init_core(cpu::local_state &cpu) {
    core_logger logger{};

    return kernel::boot::run_init_graph_logged<kernel::arch::x86_64::core_plan::core_roots>(cpu,
                                                                                            logger);
}

bool init_bsp() { return init_core(kernel::arch::x86_64::cpu::bsp()); }
}  // namespace kernel::arch::x86_64::core_init