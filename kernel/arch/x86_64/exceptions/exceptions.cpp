// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/exceptions/exceptions.hpp"

#include "kernel/arch/x86_64/cpu/cpu.hpp"
#include "kernel/arch/x86_64/idt/idt.hpp"
#include "kernel/arch/x86_64/tss/tss.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/debug/kprint.hpp"
#include "kernel/utils/traits.hpp"

namespace kernel::arch::x86_64::exceptions {

// =================================================================================================
// Assembly stubs
// =================================================================================================

extern "C" const kernel::arch::x86_64::idt::handler
    x86_64_exception_stub_table[CPU_EXCEPTION_COUNT];

// =================================================================================================
// Static checks
// =================================================================================================

static_assert(sizeof(exception_frame) == 22 * sizeof(u64));

#define DOOM_OS_EXCEPTION_FRAME_FIELD(field, index) \
    static_assert(offsetof(exception_frame, field) == (index) * sizeof(u64))

DOOM_OS_EXCEPTION_FRAME_FIELD(r15, 0);
DOOM_OS_EXCEPTION_FRAME_FIELD(r14, 1);
DOOM_OS_EXCEPTION_FRAME_FIELD(r13, 2);
DOOM_OS_EXCEPTION_FRAME_FIELD(r12, 3);
DOOM_OS_EXCEPTION_FRAME_FIELD(r11, 4);
DOOM_OS_EXCEPTION_FRAME_FIELD(r10, 5);
DOOM_OS_EXCEPTION_FRAME_FIELD(r9, 6);
DOOM_OS_EXCEPTION_FRAME_FIELD(r8, 7);
DOOM_OS_EXCEPTION_FRAME_FIELD(rbp, 8);
DOOM_OS_EXCEPTION_FRAME_FIELD(rdi, 9);
DOOM_OS_EXCEPTION_FRAME_FIELD(rsi, 10);
DOOM_OS_EXCEPTION_FRAME_FIELD(rdx, 11);
DOOM_OS_EXCEPTION_FRAME_FIELD(rcx, 12);
DOOM_OS_EXCEPTION_FRAME_FIELD(rbx, 13);
DOOM_OS_EXCEPTION_FRAME_FIELD(rax, 14);
DOOM_OS_EXCEPTION_FRAME_FIELD(vector, 15);
DOOM_OS_EXCEPTION_FRAME_FIELD(error_code, 16);
DOOM_OS_EXCEPTION_FRAME_FIELD(rip, 17);
DOOM_OS_EXCEPTION_FRAME_FIELD(cs, 18);
DOOM_OS_EXCEPTION_FRAME_FIELD(rflags, 19);
DOOM_OS_EXCEPTION_FRAME_FIELD(rsp, 20);
DOOM_OS_EXCEPTION_FRAME_FIELD(ss, 21);

#undef DOOM_OS_EXCEPTION_FRAME_FIELD

// =================================================================================================
// Exception metadata
// =================================================================================================

static constexpr const char *EXCEPTION_NAMES[CPU_EXCEPTION_COUNT] = {
    "#DE divide error",
    "#DB debug",
    "NMI interrupt",
    "#BP breakpoint",
    "#OF overflow",
    "#BR bound range exceeded",
    "#UD invalid opcode",
    "#NM device not available",
    "#DF double fault",
    "coprocessor segment overrun",
    "#TS invalid TSS",
    "#NP segment not present",
    "#SS stack-segment fault",
    "#GP general protection fault",
    "#PF page fault",
    "reserved",
    "#MF x87 floating-point exception",
    "#AC alignment check",
    "#MC machine check",
    "#XM SIMD floating-point exception",
    "#VE virtualization exception",
    "#CP control protection exception",

    // reserved
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",

    //
    "#HV hypervisor injection exception",
    "#VC VMM communication exception",
    "#SX security exception",
    "reserved",
};

const char *exception_name(u8 vector) {
    return vector < CPU_EXCEPTION_COUNT ? EXCEPTION_NAMES[vector] : "unknown exception";
}

// =================================================================================================
// Vector lists
// =================================================================================================

#define DOOM_OS_CPU_EXCEPTION_VECTORS(X) \
    X(0)                                 \
    X(1)                                 \
    X(2)                                 \
    X(3)                                 \
    X(4)                                 \
    X(5)                                 \
    X(6)                                 \
    X(7)                                 \
    X(8)                                 \
    X(9)                                 \
    X(10)                                \
    X(11)                                \
    X(12)                                \
    X(13)                                \
    X(14)                                \
    X(15)                                \
    X(16)                                \
    X(17)                                \
    X(18)                                \
    X(19)                                \
    X(20)                                \
    X(21)                                \
    X(22)                                \
    X(23)                                \
    X(24)                                \
    X(25)                                \
    X(26)                                \
    X(27)                                \
    X(28)                                \
    X(29)                                \
    X(30)                                \
    X(31)

#define DOOM_OS_WEAK_DEFAULT_EXCEPTION_VECTORS(X) \
    X(0)                                          \
    X(1)                                          \
    X(3)                                          \
    X(4)                                          \
    X(5)                                          \
    X(6)                                          \
    X(7)                                          \
    X(9)                                          \
    X(10)                                         \
    X(11)                                         \
    X(12)                                         \
    X(13)                                         \
    X(14)                                         \
    X(15)                                         \
    X(16)                                         \
    X(17)                                         \
    X(19)                                         \
    X(20)                                         \
    X(21)                                         \
    X(22)                                         \
    X(23)                                         \
    X(24)                                         \
    X(25)                                         \
    X(26)                                         \
    X(27)                                         \
    X(28)                                         \
    X(29)                                         \
    X(30)                                         \
    X(31)

// =================================================================================================
// Validation
// =================================================================================================

static constexpr bool exception_has_error_code(u8 vector) {
    switch (vector) {
        case 8:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 17:
        case 21:
        case 29:
        case 30:
            return true;
        default:
            return false;
    }
}

static void validate_exception_frame(const exception_frame &frame) {
    if (frame.vector >= CPU_EXCEPTION_COUNT)
        panic_unhandled_exception(frame);

    const auto vector = static_cast<u8>(frame.vector);
    if (!exception_has_error_code(vector) && frame.error_code != 0)
        KPANIC("[exception] malformed frame vector={} unexpected error={:#018X}", vector,
               frame.error_code);
}

static void validate_handler_vector(const exception_frame &frame, u8 expected) {
    if (frame.vector != expected)
        KPANIC("[exception] handler/vector mismatch expected={} actual={}", expected, frame.vector);
}

static const cpu::stack &fatal_exception_stack(cpu::local_state &cpu, u8 vector) {
    switch (vector) {
        case 2:
            return cpu.nmi_stack;
        case 8:
            return cpu.double_fault_stack;
        case 18:
            return cpu.machine_check_stack;
        default:
            return cpu.kernel_stack;
    }
}

// =================================================================================================
// Default handlers
// =================================================================================================

[[noreturn]] void panic_unhandled_exception(const exception_frame &frame) {
    const auto  vector = frame.vector;
    const auto *name = vector < CPU_EXCEPTION_COUNT ? EXCEPTION_NAMES[vector] : "unknown exception";

    KPANIC("[exception] {} vector={} error={:#018X}", name, vector, frame.error_code);
}

#define DOOM_OS_DEFINE_WEAK_EXCEPTION_HANDLER(vector_id)                        \
    extern "C" void DOOM_OS_EXCEPTION_HANDLER_SYMBOL(vector_id)(                \
        cpu::local_state & cpu, exception_frame & frame) __attribute__((weak)); \
                                                                                \
    extern "C" void DOOM_OS_EXCEPTION_HANDLER_SYMBOL(vector_id)(                \
        cpu::local_state & cpu [[maybe_unused]], exception_frame & frame) {     \
        static_assert(vector_id < CPU_EXCEPTION_COUNT);                         \
                                                                                \
        validate_handler_vector(frame, vector_id);                              \
                                                                                \
        panic_unhandled_exception(frame);                                       \
    }

DOOM_OS_WEAK_DEFAULT_EXCEPTION_VECTORS(DOOM_OS_DEFINE_WEAK_EXCEPTION_HANDLER)

#undef DOOM_OS_DEFINE_WEAK_EXCEPTION_HANDLER

#define DOOM_OS_DEFINE_FATAL_EXCEPTION_HANDLER(vector_id, reason)                                \
    DEFINE_EXCEPTION_HANDLER(vector_id) {                                                        \
        static_assert(vector_id < CPU_EXCEPTION_COUNT);                                          \
        const char *fatal_reason = reason;                                                       \
        const auto &fatal_stack = fatal_exception_stack(cpu, vector_id);                         \
                                                                                                 \
        validate_handler_vector(frame, vector_id);                                               \
        KPANIC(                                                                                  \
            "[exception] fatal {}: {} vector={} error={:#018X} "                                 \
            "stack=[{:#018X}, {:#018X}) size={} interrupted_rsp={:#018X}",                       \
            exception_name(static_cast<u8>(frame.vector)), fatal_reason, frame.vector,           \
            frame.error_code, fatal_stack.bottom, fatal_stack.top, fatal_stack.size, frame.rsp); \
    }

DOOM_OS_DEFINE_FATAL_EXCEPTION_HANDLER(2, "non-maskable interrupt")
DOOM_OS_DEFINE_FATAL_EXCEPTION_HANDLER(8, "double fault")
DOOM_OS_DEFINE_FATAL_EXCEPTION_HANDLER(18, "machine check")

#undef DOOM_OS_DEFINE_FATAL_EXCEPTION_HANDLER

// =================================================================================================
// Handler table
// =================================================================================================

struct exception_handler_table {
    exception_handler entries[CPU_EXCEPTION_COUNT]{};

    constexpr exception_handler operator[](const usize vector) const { return entries[vector]; }
};

using exception_vector_sequence = kernel::core::make_index_sequence<CPU_EXCEPTION_COUNT>;

#define DOOM_OS_EXCEPTION_HANDLER_ENTRY(vector) DOOM_OS_EXCEPTION_HANDLER_SYMBOL(vector),

static constexpr exception_handler_table EXCEPTION_HANDLERS{
    {DOOM_OS_CPU_EXCEPTION_VECTORS(DOOM_OS_EXCEPTION_HANDLER_ENTRY)}};

#undef DOOM_OS_EXCEPTION_HANDLER_ENTRY
#undef DOOM_OS_WEAK_DEFAULT_EXCEPTION_VECTORS
#undef DOOM_OS_CPU_EXCEPTION_VECTORS

// =================================================================================================
// IDT installation
// =================================================================================================

template <usize Vector>
static constexpr u8 ist_for_vector() {
    if constexpr (Vector == 2) {
        return static_cast<u8>(kernel::arch::x86_64::tss::interrupt_stack::NMI);
    } else if constexpr (Vector == 8) {
        return static_cast<u8>(kernel::arch::x86_64::tss::interrupt_stack::DOUBLE_FAULT);
    } else if constexpr (Vector == 18) {
        return static_cast<u8>(kernel::arch::x86_64::tss::interrupt_stack::MACHINE_CHECK);
    } else {
        return static_cast<u8>(kernel::arch::x86_64::tss::interrupt_stack::NONE);
    }
}

template <usize Vector>
static void install_exception_gate(kernel::arch::x86_64::idt::table &table) {
    constexpr auto vector = static_cast<u8>(Vector);
    constexpr auto ist = ist_for_vector<Vector>();
    const auto     handler = x86_64_exception_stub_table[Vector];

    if constexpr (Vector == 3 || Vector == 4) {
        table.set_trap_gate(vector, handler, ist);
    } else {
        table.set_interrupt_gate(vector, handler, ist);
    }
}

template <usize... Vectors>
static void install_exception_gates(kernel::arch::x86_64::idt::table &table,
                                    kernel::core::index_sequence<Vectors...>) {
    (install_exception_gate<Vectors>(table), ...);
}

bool core_component::init_component(kernel::arch::x86_64::cpu::local_state &cpu) {
    install_exception_gates(cpu.idt, exception_vector_sequence{});

    kernel::arch::x86_64::idt::load_table(cpu.idt);
    return true;
}

}  // namespace kernel::arch::x86_64::exceptions

extern "C" void x86_64_exception_dispatch(
    kernel::arch::x86_64::exceptions::exception_frame *frame) {
    using namespace kernel::arch::x86_64;

    asm volatile("cli" ::: "memory");

    if (frame == nullptr)
        KPANIC("exception dispatch received null frame");

    exceptions::validate_exception_frame(*frame);

    const auto vector = static_cast<kernel::core::u8>(frame->vector);
    const auto handler = exceptions::EXCEPTION_HANDLERS[vector];
    if (handler == nullptr)
        KPANIC("[exception] missing handler vector={}", vector);

    handler(cpu::current(), *frame);
}
