// =================================================================================================
// Kernel files
// =================================================================================================

#include "arch/x86_64/core_init.hpp"

#include "arch/x86_64/core_wiring.hpp"
#include "arch/x86_64/lapic/lapic.hpp"
#include "kernel/init/init.hpp"
#include "kernel/debug/kprint.hpp"

namespace kernel::arch::x86_64::core_init {
// =================================================================================================
// Logging
// =================================================================================================

struct core_logger {
    template <typename Component>
    void begin(cpu::local_state &cpu)
    {
        KPRINTLN("[init][core {}] {}", cpu.logical_id(), Component::name);
    }

    template <typename Component>
    void ok(cpu::local_state &cpu)
    {
        KPRINTLN("[init][core {}] {}: OK", cpu.logical_id(), Component::name);
    }

    template <typename Component>
    void fail(cpu::local_state &cpu, kernel::init::init_error error)
    {
        KPRINTLN("[init][core {}] {}: FAIL ({})", cpu.logical_id(), Component::name, error);
    }
};

// =================================================================================================
// Core initialization
// =================================================================================================

bool init_core(cpu::local_state &cpu)
{
    core_logger logger{};

    if (!kernel::init::run_init_graph<kernel::arch::x86_64::core_wiring::early_core_roots>(
            cpu, logger))
        return false;

    if (kernel::arch::x86_64::lapic::is_ready()) {
        return kernel::init::run_init_graph_after<
            kernel::arch::x86_64::core_wiring::late_core_roots,
            kernel::arch::x86_64::core_wiring::early_core_roots>(cpu, logger);
    }

    return true;
}

bool init_bsp()
{
    return init_core(kernel::arch::x86_64::cpu::bsp());
}

bool init_late_core(cpu::local_state &cpu)
{
    core_logger logger{};

    return kernel::init::run_init_graph_after<
        kernel::arch::x86_64::core_wiring::late_core_roots,
        kernel::arch::x86_64::core_wiring::early_core_roots>(cpu, logger);
}

bool init_late_bsp()
{
    return init_late_core(kernel::arch::x86_64::cpu::bsp());
}
}  // namespace kernel::arch::x86_64::core_init
