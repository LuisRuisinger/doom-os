#ifndef DOOM_OS_KERNEL_ARCH_X86_64_CORE_WIRING_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_CORE_WIRING_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/cpu/cpu.hpp"
#include "kernel/arch/x86_64/exceptions/exceptions.hpp"
#include "kernel/boot/component.hpp"

// =================================================================================================
// Resource bindings
//
// Where each core component's resource lives inside the per-core backing store. This is the
// only place that knows both halves: a module declares what it owns, the platform declares
// where it sits. Nothing else may reach local_state's members.
// =================================================================================================

namespace kernel::boot {

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

}  // namespace kernel::boot

// =================================================================================================
// Core roots
//
// The IDT is the last thing to go live on a core, so its dependency closure is the whole
// per-core graph: stacks -> tss -> gdt, exceptions -> idt.
// =================================================================================================

namespace kernel::arch::x86_64::core_wiring {

using core_roots = kernel::boot::type_list<kernel::arch::x86_64::idt::core_component>;

}  // namespace kernel::arch::x86_64::core_wiring

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_CORE_WIRING_HPP_
