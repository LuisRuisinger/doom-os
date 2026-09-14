#ifndef DOOM_OS_KERNEL_ARCH_X86_64_CORE_WIRING_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_CORE_WIRING_HPP_

#include "arch/x86_64/cpu/cpu.hpp"
#include "arch/x86_64/cpu/fpu.hpp"
#include "arch/x86_64/exceptions/exceptions.hpp"
#include "arch/x86_64/lapic/lapic.hpp"
#include "kernel/init/component.hpp"

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

namespace kernel::arch::x86_64::core_wiring {

using early_core_roots = kernel::init::type_list<kernel::arch::x86_64::idt::core_component,
                                                 kernel::arch::x86_64::cpu::fpu_component>;

using late_core_roots = kernel::init::type_list<kernel::arch::x86_64::lapic::core_component>;

using core_roots = early_core_roots;

}  // namespace kernel::arch::x86_64::core_wiring

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_CORE_WIRING_HPP_
