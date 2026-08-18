// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/memory/vmm/mmu/features.hpp"

#include "kernel/arch/x86_64/cpu/registers.hpp"

namespace kernel::core::memory::vmm::mmu {

namespace {

using kernel::arch::x86_64::cpu::CR4_PAGE_GLOBAL_ENABLE;
using kernel::arch::x86_64::cpu::EFER_NO_EXECUTE_ENABLE;
using kernel::arch::x86_64::cpu::MSR_EFER;
using kernel::core::u32;

// =================================================================================================
// CPUID leaves
// =================================================================================================

inline constexpr u32 CPUID_BASE = 0x00000001;
inline constexpr u32 CPUID_EXTENDED_MAX = 0x80000000;
inline constexpr u32 CPUID_EXTENDED_FEATURES = 0x80000001;

inline constexpr u32 EDX_PAGE_GLOBAL_ENABLE = u32{1} << 13;
inline constexpr u32 EDX_NO_EXECUTE = u32{1} << 20;
inline constexpr u32 EDX_GIB_PAGES = u32{1} << 26;

paging_features g_features{};

}  // namespace

// =================================================================================================
// Paging features
// =================================================================================================

void enable_paging_features()
{
    namespace regs = kernel::arch::x86_64::cpu;

    const u32 extended_max = regs::cpuid(CPUID_EXTENDED_MAX).eax;
    const u32 extended_edx =
        extended_max >= CPUID_EXTENDED_FEATURES ? regs::cpuid(CPUID_EXTENDED_FEATURES).edx : 0;

    g_features.nx = (extended_edx & EDX_NO_EXECUTE) != 0;
    g_features.gib_pages = (extended_edx & EDX_GIB_PAGES) != 0;
    g_features.global = (regs::cpuid(CPUID_BASE).edx & EDX_PAGE_GLOBAL_ENABLE) != 0;

    // Order matters against the encoders: a flag is only published as enabled once the control
    // register agrees, so nothing can write an NX bit into an entry before EFER.NXE is set.
    if (g_features.nx)
        regs::write_msr(MSR_EFER, regs::read_msr(MSR_EFER) | EFER_NO_EXECUTE_ENABLE);

    if (g_features.global)
        regs::write_cr4(regs::read_cr4() | CR4_PAGE_GLOBAL_ENABLE);
}

const paging_features &features()
{
    return g_features;
}

}  // namespace kernel::core::memory::vmm::mmu
