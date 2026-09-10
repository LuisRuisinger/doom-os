// =================================================================================================
// Kernel files
// =================================================================================================

#include "arch/x86_64/exceptions/exceptions.hpp"

#include "arch/x86_64/cpu/cpu.hpp"
#include "arch/x86_64/cpu/registers.hpp"
#include "arch/x86_64/idt/idt.hpp"
#include "arch/x86_64/tss/tss.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/debug/kprint.hpp"

// =================================================================================================
// Cpp stdlib files
// =================================================================================================

#include <utility>

namespace kernel::arch::x86_64::exceptions {

// =================================================================================================
// Assembly stubs
// =================================================================================================

extern "C" const kernel::arch::x86_64::idt::handler
    x86_64_exception_stub_table[CPU_EXCEPTION_COUNT];

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

const char *exception_name(u8 vector)
{
    return vector < CPU_EXCEPTION_COUNT ? EXCEPTION_NAMES[vector] : "unknown exception";
}

// =================================================================================================
// Vector lists
// =================================================================================================

#define DOOM_OS_CPU_EXCEPTION_VECTORS(X) \
    X(0, fault_exception_type)           \
    X(1, debug_exception_type)           \
    X(2, nmi_exception_type)             \
    X(3, trap_exception_type)            \
    X(4, trap_exception_type)            \
    X(5, fault_exception_type)           \
    X(6, fault_exception_type)           \
    X(7, fault_exception_type)           \
    X(8, double_fault_exception_type)    \
    X(9, fault_exception_type)           \
    X(10, fault_exception_type)          \
    X(11, fault_exception_type)          \
    X(12, fault_exception_type)          \
    X(13, fault_exception_type)          \
    X(14, fault_exception_type)          \
    X(15, reserved_exception_type)       \
    X(16, fault_exception_type)          \
    X(17, fault_exception_type)          \
    X(18, machine_check_exception_type)  \
    X(19, fault_exception_type)          \
    X(20, fault_exception_type)          \
    X(21, fault_exception_type)          \
    X(22, reserved_exception_type)       \
    X(23, reserved_exception_type)       \
    X(24, reserved_exception_type)       \
    X(25, reserved_exception_type)       \
    X(26, reserved_exception_type)       \
    X(27, reserved_exception_type)       \
    X(28, fault_exception_type)          \
    X(29, fault_exception_type)          \
    X(30, fault_exception_type)          \
    X(31, reserved_exception_type)

// =================================================================================================
// Validation
// =================================================================================================

static constexpr bool exception_has_error_code(u8 vector)
{
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

static void validate_exception_frame(const exception_frame &frame)
{
    if (frame.vector >= CPU_EXCEPTION_COUNT)
        panic_unhandled_exception(frame);

    const auto vector = static_cast<u8>(frame.vector);
    if (!exception_has_error_code(vector) && frame.error_code != 0)
        KPANIC("[exception] malformed frame vector={} unexpected error={:#018X}", vector,
               frame.error_code);
}

static void validate_handler_vector(const exception_frame &frame, u8 expected)
{
    if (frame.vector != expected)
        KPANIC("[exception] handler/vector mismatch expected={} actual={}", expected, frame.vector);
}

template <kernel::arch::x86_64::tss::interrupt_stack Stack>
static const cpu::stack &stack_for_exception_type(cpu::local_state &cpu)
{
    const cpu::stack_set &stacks = cpu.stacks();

    if constexpr (Stack == kernel::arch::x86_64::tss::interrupt_stack::NMI) {
        return stacks.nmi();
    } else if constexpr (Stack == kernel::arch::x86_64::tss::interrupt_stack::DOUBLE_FAULT) {
        return stacks.double_fault();
    } else if constexpr (Stack == kernel::arch::x86_64::tss::interrupt_stack::MACHINE_CHECK) {
        return stacks.machine_check();
    } else {
        static_assert(Stack == kernel::arch::x86_64::tss::interrupt_stack::NONE,
                      "exception type references an IST slot without a CPU stack accessor");
        return stacks.kernel();
    }
}

// =================================================================================================
// Default handlers
// =================================================================================================

// =================================================================================================
// Fault register dump
//
// KPANIC captures registers where it is written, which on a fault is the handler rather than
// the context that faulted. Capture the ambient state first - the segment and control
// registers survive the trap, and CR2 has to be read here because it carries the #PF address
// - then override every field the exception frame actually recorded.
// =================================================================================================

// The general-purpose registers come from the exception frame, which recorded them as they were
// when the fault hit. Everything else is still live in the CPU and is read back here.
static void capture_ambient_registers(kernel::debug::panic_register_frame &out)
{
#define DOOM_OS_CAPTURE(name__, asm_name__) out.name__ = kernel::arch::x86_64::cpu::read_##name__();

    DOOM_OS_X86_SEGS(DOOM_OS_CAPTURE)
    DOOM_OS_X86_CRS(DOOM_OS_CAPTURE)

#undef DOOM_OS_CAPTURE
}

static kernel::debug::panic_register_frame panic_frame_from(const exception_frame &frame)
{
    kernel::debug::panic_register_frame __panic_frame{};

    capture_ambient_registers(__panic_frame);

    __panic_frame.rax = frame.rax;
    __panic_frame.rbx = frame.rbx;
    __panic_frame.rcx = frame.rcx;
    __panic_frame.rdx = frame.rdx;
    __panic_frame.rsi = frame.rsi;
    __panic_frame.rdi = frame.rdi;
    __panic_frame.rbp = frame.rbp;
    __panic_frame.r8 = frame.r8;
    __panic_frame.r9 = frame.r9;
    __panic_frame.r10 = frame.r10;
    __panic_frame.r11 = frame.r11;
    __panic_frame.r12 = frame.r12;
    __panic_frame.r13 = frame.r13;
    __panic_frame.r14 = frame.r14;
    __panic_frame.r15 = frame.r15;

    __panic_frame.rip = frame.rip;
    __panic_frame.rsp = frame.rsp;
    __panic_frame.rflags = frame.rflags;
    __panic_frame.cs = frame.cs;
    __panic_frame.ss = frame.ss;

    return __panic_frame;
}

// =================================================================================================
// Page fault cause
//
// The #PF error code is a bitfield the manual describes one bit at a time, so a raw hex dump of
// it is the least useful thing to print at the moment a fault is killing the kernel. Decoding it
// into named fields costs nothing at runtime, and the field names are the output: the formatter
// walks this struct rather than a hand-written list that could disagree with it.
//
// Bit 0 is the odd one out - it reports a protection violation when set and a not-present page
// when clear - so it is named for what a set bit means, like the rest.
// =================================================================================================

static constexpr u64 PAGE_FAULT_VECTOR = 14;

struct page_fault_cause {
    bool protection_violation;
    bool write;
    bool user;
    bool reserved_bit;
    bool instruction_fetch;
    bool protection_key;
    bool shadow_stack;
};

static page_fault_cause decode_page_fault(u64 error_code)
{
    const auto bit = [error_code](u64 index) { return (error_code & (u64{1} << index)) != 0; };

    return page_fault_cause{
        .protection_violation = bit(0),
        .write = bit(1),
        .user = bit(2),
        .reserved_bit = bit(3),
        .instruction_fetch = bit(4),
        .protection_key = bit(5),
        .shadow_stack = bit(6),
    };
}

[[noreturn]] void panic_unhandled_exception(const exception_frame &frame)
{
    const auto  vector = frame.vector;
    const auto *name = vector < CPU_EXCEPTION_COUNT ? EXCEPTION_NAMES[vector] : "unknown exception";

    auto dump = panic_frame_from(frame);

    // capture_ambient_registers already read CR2, which is where the #PF address lives.
    if (vector == PAGE_FAULT_VECTOR) {
        KPANIC_WITH_FRAME(dump, "[exception] {} address={:#018X} error={:#x} cause={}", name,
                          dump.cr2, frame.error_code, decode_page_fault(frame.error_code));
    }

    KPANIC_WITH_FRAME(dump, "[exception] {} vector={} error={:#018X}", name, vector,
                      frame.error_code);
}

enum class exception_gate : u8 {
    INTERRUPT,
    TRAP,
};

struct fault_exception_type {
    static constexpr auto interrupt_stack = kernel::arch::x86_64::tss::interrupt_stack::NONE;
    static constexpr auto gate = exception_gate::INTERRUPT;

    template <u8 Vector>
    static void handle(cpu::local_state &cpu [[maybe_unused]], exception_frame &frame)
    {
        static_assert(Vector < CPU_EXCEPTION_COUNT);

        validate_handler_vector(frame, Vector);
        panic_unhandled_exception(frame);
    }
};

struct debug_exception_type : fault_exception_type {};

struct reserved_exception_type : fault_exception_type {};

struct trap_exception_type : fault_exception_type {
    static constexpr auto gate = exception_gate::TRAP;
};

template <typename Derived, kernel::arch::x86_64::tss::interrupt_stack Stack>
struct fatal_exception_type {
    static constexpr auto interrupt_stack = Stack;
    static constexpr auto gate = exception_gate::INTERRUPT;

    template <u8 Vector>
    static void handle(cpu::local_state &cpu, exception_frame &frame)
    {
        static_assert(Vector < CPU_EXCEPTION_COUNT);

        const auto &fatal_stack = stack_for_exception_type<Stack>(cpu);

        validate_handler_vector(frame, Vector);

        auto fatal_dump = panic_frame_from(frame);

        KPANIC_WITH_FRAME(fatal_dump,
                          "[exception] fatal {}: {} vector={} error={:#018X} "
                          "stack=[{:#018X}, {:#018X}) size={}",
                          exception_name(static_cast<u8>(frame.vector)), Derived::fatal_reason,
                          frame.vector, frame.error_code, fatal_stack.bottom, fatal_stack.top,
                          fatal_stack.size);
    }
};

struct nmi_exception_type
    : fatal_exception_type<nmi_exception_type, kernel::arch::x86_64::tss::interrupt_stack::NMI> {
    static constexpr const char *fatal_reason = "non-maskable interrupt";
};

struct double_fault_exception_type
    : fatal_exception_type<double_fault_exception_type,
                           kernel::arch::x86_64::tss::interrupt_stack::DOUBLE_FAULT> {
    static constexpr const char *fatal_reason = "double fault";
};

struct machine_check_exception_type
    : fatal_exception_type<machine_check_exception_type,
                           kernel::arch::x86_64::tss::interrupt_stack::MACHINE_CHECK> {
    static constexpr const char *fatal_reason = "machine check";
};

template <u8 Vector, typename ExceptionType>
struct exception_descriptor {
    static constexpr auto vector = Vector;
    using type = ExceptionType;

    static constexpr auto interrupt_stack = type::interrupt_stack;
    static constexpr auto gate = type::gate;

    static void handle(cpu::local_state &cpu, exception_frame &frame)
    {
        type::template handle<Vector>(cpu, frame);
    }
};

template <u8 Vector>
struct exception_descriptor_for_vector;

#define DOOM_OS_DECLARE_EXCEPTION_DESCRIPTOR(vector_id, exception_type) \
    template <>                                                        \
    struct exception_descriptor_for_vector<vector_id>                  \
        : exception_descriptor<vector_id, exception_type> {};

DOOM_OS_CPU_EXCEPTION_VECTORS(DOOM_OS_DECLARE_EXCEPTION_DESCRIPTOR)

#undef DOOM_OS_DECLARE_EXCEPTION_DESCRIPTOR

#define DOOM_OS_DEFINE_WEAK_EXCEPTION_HANDLER(vector_id, exception_type)         \
    extern "C" void DOOM_OS_EXCEPTION_HANDLER_SYMBOL(vector_id)(                \
        cpu::local_state & cpu, exception_frame & frame) __attribute__((weak)); \
                                                                                \
    extern "C" void DOOM_OS_EXCEPTION_HANDLER_SYMBOL(vector_id)(                \
        cpu::local_state & cpu [[maybe_unused]], exception_frame & frame)       \
    {                                                                           \
        exception_descriptor_for_vector<vector_id>::handle(cpu, frame);         \
    }

DOOM_OS_CPU_EXCEPTION_VECTORS(DOOM_OS_DEFINE_WEAK_EXCEPTION_HANDLER)

#undef DOOM_OS_DEFINE_WEAK_EXCEPTION_HANDLER

// =================================================================================================
// Handler table
// =================================================================================================

struct exception_handler_table {
    exception_handler entries[CPU_EXCEPTION_COUNT]{};

    constexpr exception_handler operator[](const usize vector) const
    {
        return entries[vector];
    }
};

using exception_vector_sequence = std::make_index_sequence<CPU_EXCEPTION_COUNT>;

#define DOOM_OS_EXCEPTION_HANDLER_ENTRY(vector, exception_type) \
    DOOM_OS_EXCEPTION_HANDLER_SYMBOL(vector),

static constexpr exception_handler_table EXCEPTION_HANDLERS{
    {DOOM_OS_CPU_EXCEPTION_VECTORS(DOOM_OS_EXCEPTION_HANDLER_ENTRY)}};

#undef DOOM_OS_EXCEPTION_HANDLER_ENTRY

// =================================================================================================
// IDT installation
// =================================================================================================

template <usize Vector>
static constexpr u8 ist_for_vector()
{
    using descriptor = exception_descriptor_for_vector<static_cast<u8>(Vector)>;

    return static_cast<u8>(descriptor::interrupt_stack);
}

template <usize Vector>
static void describe_exception_gate(kernel::arch::x86_64::idt::gate_table &gates)
{
    constexpr auto vector = static_cast<u8>(Vector);
    constexpr auto ist = ist_for_vector<Vector>();
    const auto     entry_point = x86_64_exception_stub_table[Vector];
    using descriptor = exception_descriptor_for_vector<vector>;

    if constexpr (descriptor::gate == exception_gate::TRAP) {
        gates.set_trap_gate(vector, entry_point, ist);
    } else {
        gates.set_interrupt_gate(vector, entry_point, ist);
    }
}

template <usize... Vectors>
static void describe_exception_gates(kernel::arch::x86_64::idt::gate_table &gates,
                                     std::index_sequence<Vectors...>)
{
    (describe_exception_gate<Vectors>(gates), ...);
}

kernel::init::init_result core_component::describe_gates(
    kernel::arch::x86_64::idt::gate_table &gates)
{
    describe_exception_gates(gates, exception_vector_sequence{});
    return kernel::core::Ok();
}

#undef DOOM_OS_CPU_EXCEPTION_VECTORS

}  // namespace kernel::arch::x86_64::exceptions

extern "C" void x86_64_exception_dispatch(kernel::arch::x86_64::trap::frame *frame)
{
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
