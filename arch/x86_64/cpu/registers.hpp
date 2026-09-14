#ifndef DOOM_OS_KERNEL_ARCH_X86_64_CPU_REGISTERS_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_CPU_REGISTERS_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/cast.hpp"
#include "kernel/core/types.hpp"

// =================================================================================================
// Register sets
//
// The one place that knows which registers exist. The accessors below are generated from these
// lists, and so is the panic register frame in debug/kpanic.hpp - so a register is added by
// editing a list here rather than by editing every place that enumerates one.
//
// The lists are split by what is actually possible, not by what is tidy: CR2 reports the address
// of the last page fault and writing it back means nothing, CS can only be changed by a far jump,
// and the general-purpose registers cannot have accessors at all - a function call clobbers the
// very registers it would be trying to report, which is why the panic capture has to be a single
// inline-asm block rather than a sequence of calls.
// =================================================================================================

#define DOOM_OS_X86_CRS(X) \
    X(cr0, "cr0")          \
    X(cr2, "cr2")          \
    X(cr3, "cr3")          \
    X(cr4, "cr4")

#define DOOM_OS_X86_CRS_WRITABLE(X) \
    X(cr0, "cr0")                   \
    X(cr3, "cr3")                   \
    X(cr4, "cr4")

#define DOOM_OS_X86_SEGS(X) \
    X(cs, "cs")             \
    X(ds, "ds")             \
    X(es, "es")             \
    X(fs, "fs")             \
    X(gs, "gs")             \
    X(ss, "ss")

#define DOOM_OS_X86_GPRS(X) \
    X(rax, "rax")           \
    X(rbx, "rbx")           \
    X(rcx, "rcx")           \
    X(rdx, "rdx")           \
    X(rsi, "rsi")           \
    X(rdi, "rdi")           \
    X(rbp, "rbp")           \
    X(rsp, "rsp")           \
    X(r8, "r8")             \
    X(r9, "r9")             \
    X(r10, "r10")           \
    X(r11, "r11")           \
    X(r12, "r12")           \
    X(r13, "r13")           \
    X(r14, "r14")           \
    X(r15, "r15")

namespace kernel::arch::x86_64::cpu {

using kernel::core::u16;
using kernel::core::u32;
using kernel::core::u64;

// =================================================================================================
// Control register bits
// =================================================================================================

inline constexpr u64 CR0_MONITOR_COPROCESSOR = u64{1} << 1;
inline constexpr u64 CR0_EMULATION = u64{1} << 2;
inline constexpr u64 CR0_WRITE_PROTECT = u64{1} << 16;

inline constexpr u64 CR4_PAGE_GLOBAL_ENABLE = u64{1} << 7;
inline constexpr u64 CR4_OS_FXSR = u64{1} << 9;
inline constexpr u64 CR4_OS_XMM_EXCEPT = u64{1} << 10;

// =================================================================================================
// Model specific registers
// =================================================================================================

inline constexpr u32 MSR_EFER = 0xC0000080;
inline constexpr u64 EFER_NO_EXECUTE_ENABLE = u64{1} << 11;

// =================================================================================================
// Control and segment registers
//
// Generated: read_cr0(), write_cr3(), read_ss() and friends. Every write carries a memory clobber
// because these change how memory is addressed or protected, so nothing the compiler cached
// across them stays valid.
// =================================================================================================

#define DOOM_OS_X86_DEFINE_READ(name__, asm_name__)             \
    [[nodiscard]] inline u64 read_##name__()                    \
    {                                                           \
        u64 value{};                                            \
        asm volatile("mov %%" asm_name__ ", %0" : "=r"(value)); \
        return value;                                           \
    }

#define DOOM_OS_X86_DEFINE_WRITE(name__, asm_name__)                     \
    inline void write_##name__(u64 value)                                \
    {                                                                    \
        asm volatile("mov %0, %%" asm_name__ : : "r"(value) : "memory"); \
    }

#define DOOM_OS_X86_DEFINE_SEG_READ(name__, asm_name__)         \
    [[nodiscard]] inline u16 read_##name__()                    \
    {                                                           \
        u16 value{};                                            \
        asm volatile("mov %%" asm_name__ ", %0" : "=r"(value)); \
        return value;                                           \
    }

DOOM_OS_X86_CRS(DOOM_OS_X86_DEFINE_READ)
DOOM_OS_X86_CRS_WRITABLE(DOOM_OS_X86_DEFINE_WRITE)
DOOM_OS_X86_SEGS(DOOM_OS_X86_DEFINE_SEG_READ)

#undef DOOM_OS_X86_DEFINE_READ
#undef DOOM_OS_X86_DEFINE_WRITE
#undef DOOM_OS_X86_DEFINE_SEG_READ

// =================================================================================================
// CPUID and MSRs
//
// Written out rather than generated: there is no list to iterate, and each has its own signature.
// The leaves and bits themselves stay with whoever cares about them; this is only the
// instruction. eax is the highest supported leaf when queried with a base leaf, so a caller
// checks that before trusting anything an extended leaf reports.
// =================================================================================================

struct cpuid_result {
    u32 eax;
    u32 ebx;
    u32 ecx;
    u32 edx;
};

[[nodiscard]] inline cpuid_result cpuid(u32 leaf, u32 subleaf = 0)
{
    cpuid_result result;
    asm volatile("cpuid"
                 : "=a"(result.eax), "=b"(result.ebx), "=c"(result.ecx), "=d"(result.edx)
                 : "a"(leaf), "c"(subleaf));

    return result;
}

[[nodiscard]] inline u64 read_msr(u32 msr)
{
    u32 low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));

    return (u64{high} << 32) | low;
}

inline void write_msr(u32 msr, u64 value)
{
    const u32 low = value as(u32);
    const u32             high = (value >> 32) as(u32);

    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high) : "memory");
}

}  // namespace kernel::arch::x86_64::cpu

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_CPU_REGISTERS_HPP_
