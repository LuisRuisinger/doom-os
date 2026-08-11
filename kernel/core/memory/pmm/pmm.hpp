#ifndef DOOM_OS_KERNEL_CORE_MEMORY_PMM_HPP_
#define DOOM_OS_KERNEL_CORE_MEMORY_PMM_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/boot_info.hpp"
#include "kernel/boot/component.hpp"
#include "kernel/core/types.hpp"

namespace kernel::core::memory::pmm {

using kernel::core::paddr_t;
using kernel::core::u32;
using kernel::core::u64;
using kernel::core::usize;

// =================================================================================================
// Constants
// =================================================================================================

inline constexpr u64 PAGE_SIZE = 4096;
inline constexpr u32 PAGE_SHIFT = 12;

inline constexpr paddr_t DMA_LIMIT = 16ull * 1024 * 1024;
inline constexpr paddr_t DMA32_LIMIT = 4ull * 1024 * 1024 * 1024;

inline constexpr u64 MAX_MANAGED_MEMORY_BYTES = 16ull * 1024 * 1024 * 1024;
inline constexpr u64 MAX_MANAGED_PAGES = MAX_MANAGED_MEMORY_BYTES / PAGE_SIZE;
inline constexpr u32 MAX_ORDER = 22;

inline constexpr paddr_t INVALID_PHYSICAL_ADDRESS = ~paddr_t{0};

static_assert((u64{1} << MAX_ORDER) == MAX_MANAGED_PAGES);

// =================================================================================================
// Zones
// =================================================================================================

enum class zone_kind : u32 {
    DMA,
    DMA32,
    NORMAL,
};

inline constexpr usize ZONE_COUNT = 3;

struct zone_stats {
    paddr_t base{};
    paddr_t limit{};
    u64     managed_pages{};
    u64     free_pages{};
    u64     allocated_pages{};
};

struct stats {
    bool       initialized{};
    u64        managed_pages{};
    u64        free_pages{};
    u64        allocated_pages{};
    zone_stats zones[ZONE_COUNT]{};
};

// =================================================================================================
// PMM API
// =================================================================================================

bool initialized();
stats current_stats();
paddr_t alloc_pages(u32 order);
paddr_t alloc_pages(zone_kind zone, u32 order);
paddr_t alloc_page();
paddr_t alloc_page(zone_kind zone);
bool free_pages(paddr_t address, u32 order);
bool free_page(paddr_t address);
bool reserve_range(paddr_t base, u64 length);
bool contains(paddr_t address);

// =================================================================================================
// Component
// =================================================================================================

struct component : kernel::boot::component<component, kernel::boot::no_resource,
                                           kernel::boot::boot_info::component> {
    static constexpr auto *name = "PMM";

    static kernel::boot::init_result init_allocator();

    template <typename View>
    static kernel::boot::init_result init(View)
    {
        return init_allocator();
    }
};

}  // namespace kernel::core::memory::pmm

#endif  // DOOM_OS_KERNEL_CORE_MEMORY_PMM_HPP_
