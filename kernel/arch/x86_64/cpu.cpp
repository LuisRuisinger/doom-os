// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/cpu.hpp"

#include "kernel/sync/atomic.hpp"

namespace kernel::arch::x86_64::cpu {

// =================================================================================================
// Storage
// =================================================================================================

static local_state               bsp_state{};
static kernel::sync::atomic<u32> current_online_count{};

// =================================================================================================
// CPU register readers
// =================================================================================================

#define DEFINE_U64_MOV_READER(FN_NAME__, REG_NAME__)            \
    static u64 FN_NAME__() {                                    \
        u64 value;                                              \
        asm volatile("mov %%" REG_NAME__ ", %0" : "=r"(value)); \
        return value;                                           \
    }

DEFINE_U64_MOV_READER(read_rax, "rax")
DEFINE_U64_MOV_READER(read_rbx, "rbx")
DEFINE_U64_MOV_READER(read_rcx, "rcx")
DEFINE_U64_MOV_READER(read_rdx, "rdx")
DEFINE_U64_MOV_READER(read_rsi, "rsi")
DEFINE_U64_MOV_READER(read_rdi, "rdi")
DEFINE_U64_MOV_READER(read_rbp, "rbp")
DEFINE_U64_MOV_READER(read_rsp, "rsp")
DEFINE_U64_MOV_READER(read_r8, "r8")
DEFINE_U64_MOV_READER(read_r9, "r9")
DEFINE_U64_MOV_READER(read_r10, "r10")
DEFINE_U64_MOV_READER(read_r11, "r11")
DEFINE_U64_MOV_READER(read_r12, "r12")
DEFINE_U64_MOV_READER(read_r13, "r13")
DEFINE_U64_MOV_READER(read_r14, "r14")
DEFINE_U64_MOV_READER(read_r15, "r15")

DEFINE_U64_MOV_READER(read_cr0, "cr0")
DEFINE_U64_MOV_READER(read_cr2, "cr2")
DEFINE_U64_MOV_READER(read_cr3, "cr3")
DEFINE_U64_MOV_READER(read_cr4, "cr4")

#undef DEFINE_U64_MOV_READER

#define DEFINE_U16_MOV_READER(FUNCTION_NAME, REGISTER_NAME)        \
    static u64 FUNCTION_NAME() {                                   \
        u16 value;                                                 \
        asm volatile("mov %%" REGISTER_NAME ", %0" : "=r"(value)); \
        return static_cast<u64>(value);                            \
    }

DEFINE_U16_MOV_READER(read_cs, "cs")
DEFINE_U16_MOV_READER(read_ds, "ds")
DEFINE_U16_MOV_READER(read_es, "es")
DEFINE_U16_MOV_READER(read_fs, "fs")
DEFINE_U16_MOV_READER(read_gs, "gs")
DEFINE_U16_MOV_READER(read_ss, "ss")

#undef DEFINE_U16_MOV_READER

static u64 read_rip() {
    u64 value;

    asm volatile("leaq 0(%%rip), %0" : "=r"(value));

    return value;
}

static u64 read_rflags() {
    u64 value;

    asm volatile(
        "pushfq\n"
        "pop %0\n"
        : "=r"(value)
        :
        : "memory");

    return value;
}

// =================================================================================================
// CPU register descriptors
// =================================================================================================

const cpu_register_descriptor cpu_register_descriptors[CPU_REGISTER_COUNT] = {
#define REGISTER_DESCRIPTOR_ENTRY(NAME, STRING_NAME, READER) \
    {cpu_register::NAME, STRING_NAME, READER},

    DOOM_OS_X86_64_CPU_REGISTER_LIST(REGISTER_DESCRIPTOR_ENTRY)

#undef REGISTER_DESCRIPTOR_ENTRY
};

u64 read_current_register(cpu_register reg) {
    const auto index = static_cast<usize>(reg);

    return index < CPU_REGISTER_COUNT ? cpu_register_descriptors[index].read() : 0;
}

const char *cpu_register_name(cpu_register reg) {
    const auto index = static_cast<usize>(reg);

    return index < CPU_REGISTER_COUNT ? cpu_register_descriptors[index].name : "unknown";
}

usize read_current_registers(cpu_register_value *out, usize capacity) {
    if (!out || capacity < CPU_REGISTER_COUNT)
        return CPU_REGISTER_COUNT;

    for (usize i = 0; i < CPU_REGISTER_COUNT; ++i) {
        const cpu_register_descriptor &descriptor = cpu_register_descriptors[i];

        out[i] = cpu_register_value{descriptor.reg, descriptor.name, descriptor.read()};
    }

    return CPU_REGISTER_COUNT;
}

// =================================================================================================
// CPU-local stack descriptor
// =================================================================================================

stack::stack(u8 *storage, usize storage_size)
    : bottom(reinterpret_cast<u64>(storage)),
      top(reinterpret_cast<u64>(storage) + storage_size),
      size(storage_size) {}

// =================================================================================================
// Local CPU state
// =================================================================================================

void local_state::init_stack_descriptors() {
    kernel_stack = stack{kernel_stack_storage, CORE_STACK_SIZE};
    double_fault_stack = stack{double_fault_stack_storage, CORE_STACK_SIZE};
    nmi_stack = stack{nmi_stack_storage, CORE_STACK_SIZE};
    machine_check_stack = stack{machine_check_stack_storage, CORE_STACK_SIZE};
}

void local_state::init_task_state_segment() {
    task_state_segment.init(kernel::arch::x86_64::tss::stack_config{
        .rsp0 = kernel_stack_top(),
        .ist1 = double_fault_stack_top(),
        .ist2 = nmi_stack_top(),
        .ist3 = machine_check_stack_top(),
    });
}

u64 local_state::kernel_stack_top() const { return kernel_stack.top; }

u64 local_state::double_fault_stack_top() const { return double_fault_stack.top; }

u64 local_state::nmi_stack_top() const { return nmi_stack.top; }

u64 local_state::machine_check_stack_top() const { return machine_check_stack.top; }

// =================================================================================================
// BSP
// =================================================================================================

void init_bsp() {
    bsp_state.logical_id = 0;
    bsp_state.apic_id = 0;
    bsp_state.is_bsp = true;
    bsp_state.is_online = true;
    bsp_state.init_stack_descriptors();

    current_online_count.fetch_add(1);
}

local_state &bsp() { return bsp_state; }

// =================================================================================================
// CPU state storage
// =================================================================================================

local_state *get(u32 logical_id) { return logical_id == 0 ? &bsp_state : nullptr; }

u32 online_count() { return current_online_count.load(sync::memory_order::ACQUIRE); }

}  // namespace kernel::arch::x86_64::cpu
