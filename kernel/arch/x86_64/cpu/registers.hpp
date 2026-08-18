#ifndef DOOM_OS_KERNEL_ARCH_X86_64_CPU_REGISTERS_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_CPU_REGISTERS_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"

namespace kernel::arch::x86_64::cpu {

using kernel::core::u32;
using kernel::core::u64;

// =================================================================================================
// Control register bits
// =================================================================================================

inline constexpr u64 CR0_WRITE_PROTECT = u64{1} << 16;
inline constexpr u64 CR4_PAGE_GLOBAL_ENABLE = u64{1} << 7;

// =================================================================================================
// Model specific registers
// =================================================================================================

inline constexpr u32 MSR_EFER = 0xC0000080;
inline constexpr u64 EFER_NO_EXECUTE_ENABLE = u64{1} << 11;

// =================================================================================================
// CPUID
//
// The leaves and bits themselves stay with whoever cares about them; this is only the
// instruction. eax is the highest supported leaf when queried with a base leaf, so a caller
// checks that before trusting anything an extended leaf reports.
// =================================================================================================

struct cpuid_result {
    u32 eax{};
    u32 ebx{};
    u32 ecx{};
    u32 edx{};
};

[[nodiscard]] inline cpuid_result cpuid(u32 leaf, u32 subleaf = 0)
{
    cpuid_result result{};

    asm volatile("cpuid"
                 : "=a"(result.eax), "=b"(result.ebx), "=c"(result.ecx), "=d"(result.edx)
                 : "a"(leaf), "c"(subleaf));

    return result;
}

// =================================================================================================
// Control registers
//
// Every write carries a memory clobber: these change how memory is addressed or protected, so
// nothing the compiler cached across them stays valid.
// =================================================================================================

[[nodiscard]] inline u64 read_cr0()
{
    u64 value{};

    asm volatile("mov %%cr0, %0" : "=r"(value));

    return value;
}

inline void write_cr0(u64 value)
{
    asm volatile("mov %0, %%cr0" : : "r"(value) : "memory");
}

[[nodiscard]] inline u64 read_cr3()
{
    u64 value{};

    asm volatile("mov %%cr3, %0" : "=r"(value));

    return value;
}

inline void write_cr3(u64 value)
{
    asm volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}

[[nodiscard]] inline u64 read_cr4()
{
    u64 value{};

    asm volatile("mov %%cr4, %0" : "=r"(value));

    return value;
}

inline void write_cr4(u64 value)
{
    asm volatile("mov %0, %%cr4" : : "r"(value) : "memory");
}

// =================================================================================================
// Model specific registers
// =================================================================================================

[[nodiscard]] inline u64 read_msr(u32 msr)
{
    u32 low{};
    u32 high{};

    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));

    return (u64{high} << 32) | low;
}

inline void write_msr(u32 msr, u64 value)
{
    const u32 low = static_cast<u32>(value);
    const u32 high = static_cast<u32>(value >> 32);

    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high) : "memory");
}

}  // namespace kernel::arch::x86_64::cpu

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_CPU_REGISTERS_HPP_
