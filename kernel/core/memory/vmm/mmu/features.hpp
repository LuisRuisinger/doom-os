#ifndef DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_FEATURES_HPP_
#define DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_FEATURES_HPP_

namespace kernel::core::memory::vmm::mmu {

// =================================================================================================
// Paging feature state
// =================================================================================================

// Detects what the CPU offers, enables what it can, and publishes the result. Idempotent.
void enable_paging_features();

[[nodiscard]] bool nx_enabled();
[[nodiscard]] bool global_pages_enabled();
[[nodiscard]] bool gib_pages_supported();

}  // namespace kernel::core::memory::vmm::mmu

#endif  // DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_FEATURES_HPP_
