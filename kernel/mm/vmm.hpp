#ifndef DOOM_OS_KERNEL_MM_VMM_HPP_
#define DOOM_OS_KERNEL_MM_VMM_HPP_

// =================================================================================================
// Config files
// =================================================================================================

#include "config/layout.h"

// =================================================================================================
// Kernel files
// =================================================================================================

#include "arch/x86_64/mmu/direct_map.hpp"
#include "arch/x86_64/mmu/features.hpp"
#include "arch/x86_64/mmu/mmu.hpp"
#include "kernel/core/result.hpp"
#include "kernel/core/types.hpp"
#include "kernel/init/component.hpp"
#include "kernel/mm/page.hpp"

namespace kernel::mm::vmm {

using kernel::core::paddr_t;
using kernel::core::Result;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;
using kernel::core::vaddr_t;

using kernel::arch::x86_64::mmu::address_space;
using kernel::arch::x86_64::mmu::mapping;
using kernel::arch::x86_64::mmu::page_flags;
using kernel::mm::page_size;

// =================================================================================================
// Error types & Page Flag Presets
// =================================================================================================

enum class vmm_error : u8 {
    INVALID_ARGUMENT,
    OUT_OF_ADDRESS_SPACE,
    NO_PHYSICAL_MEMORY,
    PAGE_TABLE_CONFLICT,
    MAPPING_FAILED,
    UNMAPPING_FAILED,
    NOT_MAPPED,
};

[[nodiscard]] inline page_flags flags_kernel_data()
{
    page_flags f{};
    f.word(0) = kernel::arch::x86_64::mmu::PAGE_FLAG_WRITABLE |
                kernel::arch::x86_64::mmu::PAGE_FLAG_GLOBAL |
                kernel::arch::x86_64::mmu::PAGE_FLAG_NO_EXECUTE;
    return f;
}

[[nodiscard]] inline page_flags flags_kernel_code()
{
    page_flags f{};
    f.word(0) = kernel::arch::x86_64::mmu::PAGE_FLAG_GLOBAL;
    return f;
}

[[nodiscard]] inline page_flags flags_mmio()
{
    page_flags f{};
    f.word(0) = kernel::arch::x86_64::mmu::PAGE_FLAG_WRITABLE |
                kernel::arch::x86_64::mmu::PAGE_FLAG_GLOBAL |
                kernel::arch::x86_64::mmu::PAGE_FLAG_CACHE_DISABLE |
                kernel::arch::x86_64::mmu::PAGE_FLAG_NO_EXECUTE;
    return f;
}

// =================================================================================================
// Hardware Translation Leaf Support (for THP and large page allocation)
// =================================================================================================

[[nodiscard]] inline bool supports_leaf(page_size size)
{
    switch (size) {
        case page_size::SIZE_4K:
        case page_size::SIZE_2M:
            return true;
        case page_size::SIZE_1G:
            return kernel::arch::x86_64::mmu::gib_pages_supported();
    }
    return false;
}

// =================================================================================================
// Physical Direct-Map Window
// =================================================================================================

[[nodiscard]] inline void *physical_window(paddr_t physical_address)
{
    return kernel::arch::x86_64::mmu::physical_window(physical_address);
}

[[nodiscard]] inline void *physical_window(paddr_t physical_address, usize size)
{
    namespace arch_mmu = kernel::arch::x86_64::mmu;

    if (size == 0)
        return nullptr;

    const auto limit = arch_mmu::direct_map_ready() ? arch_mmu::DIRECT_MAP_SIZE
                                                    : arch_mmu::EARLY_MAP_SIZE;

    if (physical_address > limit || size > limit - physical_address)
        return nullptr;

    return physical_window(physical_address);
}

// =================================================================================================
// Kernel Address Space Operations
// =================================================================================================

[[nodiscard]] address_space &kernel_address_space();

[[nodiscard]] Result<void, vmm_error> map(vaddr_t virtual_address, paddr_t physical_address,
                                          page_size size, page_flags flags);

[[nodiscard]] Result<void, vmm_error> map_range(vaddr_t virtual_address, paddr_t physical_address,
                                                u64 length, page_size size, page_flags flags);

[[nodiscard]] Result<void, vmm_error> unmap(vaddr_t virtual_address, page_size size);

[[nodiscard]] Result<mapping, vmm_error> translate(vaddr_t virtual_address);

// =================================================================================================
// Dedicated MMIO Allocator
// =================================================================================================

[[nodiscard]] Result<vaddr_t, vmm_error> map_mmio(paddr_t physical_address, usize bytes);

// =================================================================================================
// Component
// =================================================================================================

struct component : kernel::init::component<component, kernel::init::no_resource,
                                           kernel::arch::x86_64::mmu::component> {
    static constexpr auto *name = "VMM";

    static kernel::init::init_result init_vmm();

    template <typename View>
    static kernel::init::init_result init(View)
    {
        return init_vmm();
    }
};

}  // namespace kernel::mm::vmm

#endif  // DOOM_OS_KERNEL_MM_VMM_HPP_