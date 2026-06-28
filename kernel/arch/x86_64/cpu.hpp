#ifndef DOOM_OS_KERNEL_ARCH_X86_64_CPU_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_CPU_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/gdt.hpp"
#include "kernel/arch/x86_64/idt.hpp"
#include "kernel/arch/x86_64/tss.hpp"
#include "kernel/boot/component.hpp"
#include "kernel/core/types.hpp"

namespace kernel::arch::x86_64::cpu {

using kernel::core::u16;
using kernel::core::u32;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

// =================================================================================================
// Constants
// =================================================================================================

static constexpr usize CORE_STACK_SIZE = 16 * 1024;
static constexpr u32   MAX_CORE_COUNT = 64;
static constexpr u32   STACK_BYTE_ALIGNMNT = 16;

// =================================================================================================
// CPU register list
// =================================================================================================

#define DOOM_OS_X86_64_CPU_REGISTER_LIST(X) \
    X(RAX, "rax", read_rax)                 \
    X(RBX, "rbx", read_rbx)                 \
    X(RCX, "rcx", read_rcx)                 \
    X(RDX, "rdx", read_rdx)                 \
    X(RSI, "rsi", read_rsi)                 \
    X(RDI, "rdi", read_rdi)                 \
    X(RBP, "rbp", read_rbp)                 \
    X(RSP, "rsp", read_rsp)                 \
    X(R8, "r8", read_r8)                    \
    X(R9, "r9", read_r9)                    \
    X(R10, "r10", read_r10)                 \
    X(R11, "r11", read_r11)                 \
    X(R12, "r12", read_r12)                 \
    X(R13, "r13", read_r13)                 \
    X(R14, "r14", read_r14)                 \
    X(R15, "r15", read_r15)                 \
    X(RIP, "rip", read_rip)                 \
    X(RFLAGS, "rflags", read_rflags)        \
    X(CS, "cs", read_cs)                    \
    X(DS, "ds", read_ds)                    \
    X(ES, "es", read_es)                    \
    X(FS, "fs", read_fs)                    \
    X(GS, "gs", read_gs)                    \
    X(SS, "ss", read_ss)                    \
    X(CR0, "cr0", read_cr0)                 \
    X(CR2, "cr2", read_cr2)                 \
    X(CR3, "cr3", read_cr3)                 \
    X(CR4, "cr4", read_cr4)

// =================================================================================================
// CPU registers
// =================================================================================================

enum class cpu_register : u32 {
#define ENUM_ENTRY(NAME, STRING_NAME, READER) NAME,
    DOOM_OS_X86_64_CPU_REGISTER_LIST(ENUM_ENTRY)
#undef ENUM_ENTRY

        COUNT
};

static constexpr usize CPU_REGISTER_COUNT = static_cast<usize>(cpu_register::COUNT);
using cpu_register_reader = u64 (*)();

struct cpu_register_descriptor {
    cpu_register        reg;
    const char         *name;
    cpu_register_reader read;
};

struct cpu_register_value {
    cpu_register reg;
    const char  *name;
    u64          value;
};

extern const cpu_register_descriptor cpu_register_descriptors[CPU_REGISTER_COUNT];

u64         read_current_register(cpu_register reg);
const char *cpu_register_name(cpu_register reg);
usize       read_current_registers(cpu_register_value *out, usize capacity);

// =================================================================================================
// CPU-local stack descriptor
// =================================================================================================

struct stack {
    stack() = default;
    stack(u8 *storage, usize storage_size);

    u64   bottom{};
    u64   top{};
    usize size{};
};

// =================================================================================================
// Local CPU state
// =================================================================================================

struct local_state {
    u32  logical_id;
    u32  apic_id;
    bool is_bsp;
    bool is_online;

    gdt::table gdt;
    idt::table idt;
    tss::state task_state_segment;

    stack kernel_stack;
    stack double_fault_stack;
    stack nmi_stack;
    stack machine_check_stack;

    alignas(STACK_BYTE_ALIGNMNT) u8 kernel_stack_storage[CORE_STACK_SIZE];
    alignas(STACK_BYTE_ALIGNMNT) u8 double_fault_stack_storage[CORE_STACK_SIZE];
    alignas(STACK_BYTE_ALIGNMNT) u8 nmi_stack_storage[CORE_STACK_SIZE];
    alignas(STACK_BYTE_ALIGNMNT) u8 machine_check_stack_storage[CORE_STACK_SIZE];

    void init_stack_descriptors();
    void init_task_state_segment();

    u64 kernel_stack_top() const;
    u64 double_fault_stack_top() const;
    u64 nmi_stack_top() const;
    u64 machine_check_stack_top() const;
};

// =================================================================================================
// CPU state storage
// =================================================================================================

void         init_bsp();
local_state &bsp();
local_state *get(u32 logical_id);
u32          online_count();

static inline void relax() {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#else
    asm volatile("" ::: "memory");
#endif
}

// =================================================================================================
// Component
// =================================================================================================

struct component : kernel::boot::component_base<component> {
    static constexpr auto *name = "CPU";

    static bool init_component() {
        init_bsp();
        return true;
    }
};

}  // namespace kernel::arch::x86_64::cpu

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_CPU_HPP_
