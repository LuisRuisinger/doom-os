#ifndef DOOM_OS_KERNEL_ARCH_X86_64_CPU_REGISTERS_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_CPU_REGISTERS_HPP_

#include "arch/common/cpu/registers.hpp"
#include "kernel/core/cast.hpp"
#include "kernel/core/types.hpp"

#define DOOM_OS_X86_CRS(X) \
    X(cr0)                 \
    X(cr2)                 \
    X(cr3)                 \
    X(cr4)

#define DOOM_OS_X86_CRS_WRITABLE(X) \
    X(cr0)                          \
    X(cr3)                          \
    X(cr4)

#define DOOM_OS_X86_SEGS(X) \
    X(cs)                   \
    X(ds)                   \
    X(es)                   \
    X(fs)                   \
    X(gs)                   \
    X(ss)

#define DOOM_OS_X86_GPRS(X) \
    X(rax)                  \
    X(rbx)                  \
    X(rcx)                  \
    X(rdx)                  \
    X(rsi)                  \
    X(rdi)                  \
    X(rbp)                  \
    X(rsp)                  \
    X(r8)                   \
    X(r9)                   \
    X(r10)                  \
    X(r11)                  \
    X(r12)                  \
    X(r13)                  \
    X(r14)                  \
    X(r15)

namespace kernel::arch::x86_64::cpu {

using kernel::core::u16;
using kernel::core::u32;
using kernel::core::u64;

#define DOOM_OS_X86_DEFINE_READ(name__) DOOM_OS_REGISTER_READ(u64, name__, "mov %%" #name__ ", %0")

#define DOOM_OS_X86_DEFINE_SEG_READ(name__) \
    DOOM_OS_REGISTER_READ(u16, name__, "mov %%" #name__ ", %0")

#define DOOM_OS_X86_DEFINE_WRITE(name__) DOOM_OS_REGISTER_WRITE(u64, name__, "mov %0, %%" #name__)

DOOM_OS_X86_CRS(DOOM_OS_X86_DEFINE_READ)
DOOM_OS_X86_CRS_WRITABLE(DOOM_OS_X86_DEFINE_WRITE)
DOOM_OS_X86_SEGS(DOOM_OS_X86_DEFINE_SEG_READ)

#undef DOOM_OS_X86_DEFINE_READ
#undef DOOM_OS_X86_DEFINE_SEG_READ
#undef DOOM_OS_X86_DEFINE_WRITE

inline constexpr u64 CR0_MONITOR_COPROCESSOR = u64{1} << 1;
inline constexpr u64 CR0_EMULATION = u64{1} << 2;
inline constexpr u64 CR0_WRITE_PROTECT = u64{1} << 16;

inline constexpr u64 CR4_PAGE_GLOBAL_ENABLE = u64{1} << 7;
inline constexpr u64 CR4_OS_FXSR = u64{1} << 9;
inline constexpr u64 CR4_OS_XMM_EXCEPT = u64{1} << 10;

inline constexpr u32 MSR_EFER = 0xC0000080;
inline constexpr u64 EFER_NO_EXECUTE_ENABLE = u64{1} << 11;

[[nodiscard]] inline u64 read_msr(u32 msr)
{
    u32 low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));

    return (u64{high} << 32) | low;
}

inline void write_msr(u32 msr, u64 value)
{
    asm volatile("wrmsr" : : "c"(msr), "a"(value as(u32)), "d"((value >> 32) as(u32)) : "memory");
}

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

}  // namespace kernel::arch::x86_64::cpu

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_CPU_REGISTERS_HPP_
