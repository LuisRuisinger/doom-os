#ifndef DOOM_OS_ARCH_X86_64_MMU_FEATURES_HPP_
#define DOOM_OS_ARCH_X86_64_MMU_FEATURES_HPP_

namespace kernel::arch::x86_64::mmu {

void enable_paging_features();

[[nodiscard]] bool nx_enabled();
[[nodiscard]] bool global_pages_enabled();
[[nodiscard]] bool gib_pages_supported();

}  // namespace kernel::arch::x86_64::mmu

#endif  // DOOM_OS_ARCH_X86_64_MMU_FEATURES_HPP_
