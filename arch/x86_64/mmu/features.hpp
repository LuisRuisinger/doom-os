#ifndef DOOM_OS_ARCH_X86_64_MMU_FEATURES_HPP_
#define DOOM_OS_ARCH_X86_64_MMU_FEATURES_HPP_

namespace kernel::arch::x86_64::mmu {

// =================================================================================================
// Paging feature state
// =================================================================================================

// Detects what the CPU offers, enables what it can, and publishes the result. Idempotent.
void enable_paging_features();

[[nodiscard]] bool nx_enabled();
[[nodiscard]] bool global_pages_enabled();
[[nodiscard]] bool gib_pages_supported();

}  // namespace kernel::arch::x86_64::mmu

#endif  // DOOM_OS_ARCH_X86_64_MMU_FEATURES_HPP_
