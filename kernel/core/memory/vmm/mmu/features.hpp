#ifndef DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_FEATURES_HPP_
#define DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_FEATURES_HPP_

namespace kernel::core::memory::vmm::mmu {

// =================================================================================================
// Paging features
//
// Every flag here means "supported by this CPU and switched on", never merely supported. The two
// cannot be separated: an entry carrying the NX bit while EFER.NXE is clear is a reserved-bit
// fault rather than a non-executable page, so whoever encodes entries has to see the same answer
// as whoever wrote the control registers.
//
// This sits below both the page-table walker and the address-space layer because both need it and
// neither should have to reach up to mmu.hpp for it.
// =================================================================================================

struct paging_features {
    bool nx{};
    bool global{};
    bool gib_pages{};
};

// Detects what the CPU offers, enables what it can, and publishes the result. Idempotent.
void enable_paging_features();

[[nodiscard]] const paging_features &features();

}  // namespace kernel::core::memory::vmm::mmu

#endif  // DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_FEATURES_HPP_
