#ifndef DOOM_OS_KERNEL_ARCH_X86_64_EXCEPTIONS_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_EXCEPTIONS_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "arch/x86_64/idt/idt.hpp"
#include "arch/x86_64/trap/frame.hpp"
#include "kernel/core/types.hpp"
#include "kernel/init/component.hpp"

namespace kernel::arch::x86_64::cpu {

class local_state;

}  // namespace kernel::arch::x86_64::cpu

namespace kernel::arch::x86_64::exceptions {

using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

// =================================================================================================
// Constants
// =================================================================================================

static constexpr u8 CPU_EXCEPTION_COUNT = 32;

// =================================================================================================
// Exception frame
// =================================================================================================

using exception_frame = kernel::arch::x86_64::trap::frame;

const char *exception_name(u8 vector);

using exception_handler = void (*)(cpu::local_state &cpu, exception_frame &frame);

[[noreturn]] void panic_unhandled_exception(const exception_frame &frame);

// =================================================================================================
// Components provide exception hooks by defining the strong C symbol for a
// vector. Missing handlers resolve to weak defaults that panic.
//
// Example:
//
//  DEFINE_EXCEPTION_HANDLER(14) {
//      page fault handler; use cpu for per-core state and frame for the saved registers
//  }
//
// Returning from a custom handler resumes the interrupted context with iretq.
// Call panic_unhandled_exception(frame) to delegate to the default fatal path.
// =================================================================================================

#define DOOM_OS_CONCAT_INNER(lhs, rhs) lhs##rhs
#define DOOM_OS_CONCAT(lhs, rhs)       DOOM_OS_CONCAT_INNER(lhs, rhs)

#define DOOM_OS_EXCEPTION_HANDLER_SYMBOL(vector) \
    DOOM_OS_CONCAT(doom_os_exception_handler_vector_, vector)

#define DEFINE_EXCEPTION_HANDLER(vector)                                 \
    extern "C" void DOOM_OS_EXCEPTION_HANDLER_SYMBOL(vector)(            \
        ::kernel::arch::x86_64::cpu::local_state & cpu,                  \
        ::kernel::arch::x86_64::exceptions::exception_frame & frame);    \
                                                                         \
    extern "C" void DOOM_OS_EXCEPTION_HANDLER_SYMBOL(vector)(            \
        ::kernel::arch::x86_64::cpu::local_state & cpu [[maybe_unused]], \
        ::kernel::arch::x86_64::exceptions::exception_frame & frame [[maybe_unused]])

// =================================================================================================
// Core component
//
// Owns the description of the CPU exception gates, not the IDT itself. The IDT depends on
// this table and installs it; nothing here reaches into the IDT's own state.
// =================================================================================================

struct core_component : kernel::init::component<core_component, idt::gate_table> {
    static constexpr auto *name = "EXCEPTIONS";

    static kernel::init::init_result describe_gates(idt::gate_table &gates);

    template <typename View>
    static kernel::init::init_result init(View view)
    {
        return describe_gates(own(view));
    }
};

}  // namespace kernel::arch::x86_64::exceptions

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_EXCEPTIONS_HPP_
