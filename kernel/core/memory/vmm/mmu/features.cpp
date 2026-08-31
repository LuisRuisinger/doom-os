// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/memory/vmm/mmu/features.hpp"

#include "kernel/arch/x86_64/cpu/registers.hpp"
#include "kernel/core/memory/page.hpp"

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

bool g_nx_enabled{};
bool g_global_pages_enabled{};

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

    const bool nx_supported = (extended_edx & EDX_NO_EXECUTE) != 0;
    const bool global_pages_supported = (regs::cpuid(CPUID_BASE).edx & EDX_PAGE_GLOBAL_ENABLE) != 0;

    if (nx_supported) {
        regs::write_msr(MSR_EFER, regs::read_msr(MSR_EFER) | EFER_NO_EXECUTE_ENABLE);
        g_nx_enabled = true;
    }

    if (global_pages_supported) {
        regs::write_cr4(regs::read_cr4() | CR4_PAGE_GLOBAL_ENABLE);
        g_global_pages_enabled = true;
    }

    kernel::core::memory::set_hw_allocatable(page_size::SIZE_1G,
                                             (extended_edx & EDX_GIB_PAGES) != 0);
}

bool nx_enabled()
{
    return g_nx_enabled;
}

bool global_pages_enabled()
{
    return g_global_pages_enabled;
}

bool gib_pages_supported()
{
    return is_hw_allocatable(page_size::SIZE_1G) == page_hw_allocatability::YES;
}

}  // namespace kernel::core::memory::vmm::mmu
