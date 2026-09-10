#ifndef DOOM_OS_KERNEL_ARCH_X86_64_CORE_WIRING_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_CORE_WIRING_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "arch/x86_64/cpu/cpu.hpp"
#include "arch/x86_64/cpu/fpu.hpp"
#include "arch/x86_64/exceptions/exceptions.hpp"
#include "arch/x86_64/lapic/lapic.hpp"
#include "kernel/init/component.hpp"

// =================================================================================================
// Resource bindings
//
// Where each core component's resource lives inside the per-core backing store. This is the
// only place that knows both halves: a module declares what it owns, the platform declares
// where it sits. Nothing else may reach local_state's members.
// =================================================================================================

namespace kernel::init {

#define DOOM_OS_BIND_CORE_RESOURCE(component_type, resource_type, member)             \
    template <>                                                                       \
    struct resource_binding<component_type, kernel::arch::x86_64::cpu::local_state> { \
        static resource_type &get(kernel::arch::x86_64::cpu::local_state &state)      \
        {                                                                             \
            return state.member;                                                      \
        }                                                                             \
    }

DOOM_OS_BIND_CORE_RESOURCE(kernel::arch::x86_64::cpu::stacks_component,
                           kernel::arch::x86_64::cpu::stack_set, stacks_m);

DOOM_OS_BIND_CORE_RESOURCE(kernel::arch::x86_64::tss::core_component,
                           kernel::arch::x86_64::tss::state, tss_m);

DOOM_OS_BIND_CORE_RESOURCE(kernel::arch::x86_64::gdt::core_component,
                           kernel::arch::x86_64::gdt::table, gdt_m);

DOOM_OS_BIND_CORE_RESOURCE(kernel::arch::x86_64::exceptions::core_component,
                           kernel::arch::x86_64::idt::gate_table, exception_gates_m);

DOOM_OS_BIND_CORE_RESOURCE(kernel::arch::x86_64::idt::core_component,
                           kernel::arch::x86_64::idt::table, idt_m);

#undef DOOM_OS_BIND_CORE_RESOURCE

}  // namespace kernel::init

// =================================================================================================
// Core roots
//
// The IDT is the last thing to go live on a core, so its dependency closure is the whole
// per-core graph: stacks -> tss -> gdt, exceptions -> idt.
//
// The FPU is a second root rather than a link in that chain. It depends on nothing and nothing
// depends on it - no kernel code uses x87 or SSE - so hanging it off the IDT closure would state
// an ordering that does not exist.
// =================================================================================================

// LAPIC is a late core component because its MMIO page is discovered from ACPI and mapped by
// the boot graph. It still declares its IDT dependency, and the late runner filters the early
// closure instead of rerunning it.
// =================================================================================================

namespace kernel::arch::x86_64::core_wiring {

using early_core_roots = kernel::init::type_list<kernel::arch::x86_64::idt::core_component,
                                                 kernel::arch::x86_64::cpu::fpu_component>;

using late_core_roots = kernel::init::type_list<kernel::arch::x86_64::lapic::core_component>;

using core_roots = early_core_roots;

}  // namespace kernel::arch::x86_64::core_wiring

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_CORE_WIRING_HPP_
