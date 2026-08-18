#ifndef DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_PAGETABLE_HPP_
#define DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_PAGETABLE_HPP_

// =================================================================================================
// Config files
// =================================================================================================

#include "config/layout.h"

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"

namespace kernel::core::memory::vmm::mmu {

using kernel::core::paddr_t;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;
using kernel::core::vaddr_t;

// =================================================================================================
// Paging constants
// =================================================================================================

inline constexpr usize ENTRIES_PER_TABLE = 512;

inline constexpr u64 PAGE_SHIFT_4K = 12;
inline constexpr u64 PAGE_SHIFT_2M = 21;
inline constexpr u64 PAGE_SHIFT_1G = 30;
inline constexpr u64 PML4_SLOT_SIZE = u64{1} << 39;

inline constexpr u64 PAGE_SIZE_4K = u64{1} << PAGE_SHIFT_4K;
inline constexpr u64 PAGE_SIZE_2M = u64{1} << PAGE_SHIFT_2M;
inline constexpr u64 PAGE_SIZE_1G = u64{1} << PAGE_SHIFT_1G;

inline constexpr vaddr_t KERNEL_BASE = static_cast<vaddr_t>(DOOM_OS_KERNEL_VMA);
inline constexpr vaddr_t DIRECT_MAP_BASE = static_cast<vaddr_t>(DOOM_OS_DIRECT_MAP_BASE);
inline constexpr u64     DIRECT_MAP_SIZE = static_cast<u64>(DOOM_OS_MAX_PHYSICAL_MEMORY);

static_assert((KERNEL_BASE & (PAGE_SIZE_1G - 1)) == 0, "kernel base must be 1 GiB aligned");
static_assert((DIRECT_MAP_BASE & (PML4_SLOT_SIZE - 1)) == 0,
              "direct map base must be 512 GiB aligned");
static_assert(DIRECT_MAP_SIZE % PAGE_SIZE_2M == 0,
              "direct map size must be expressible as 2 MiB leaves");

enum class page_size : u8 {
    SIZE_4K,
    SIZE_2M,
    SIZE_1G,
};

[[nodiscard]] inline constexpr u64 bytes_in(page_size size)
{
    switch (size) {
        case page_size::SIZE_4K:
            return PAGE_SIZE_4K;
        case page_size::SIZE_2M:
            return PAGE_SIZE_2M;
        case page_size::SIZE_1G:
            return PAGE_SIZE_1G;
    }

    return 0;
}

[[nodiscard]] inline constexpr usize pml4_index(vaddr_t address)
{
    return static_cast<usize>((address >> 39) & 0x1FF);
}

[[nodiscard]] inline constexpr usize pdpt_index(vaddr_t address)
{
    return static_cast<usize>((address >> 30) & 0x1FF);
}

[[nodiscard]] inline constexpr usize pd_index(vaddr_t address)
{
    return static_cast<usize>((address >> 21) & 0x1FF);
}

[[nodiscard]] inline constexpr usize pt_index(vaddr_t address)
{
    return static_cast<usize>((address >> 12) & 0x1FF);
}

// =================================================================================================
// Page attributes
// =================================================================================================

struct page_flags {
    bool writable{};
    bool executable{};
    bool user{};
    bool global{};
    bool write_through{};
    bool cache_disable{};
};

struct mapping {
    bool       present{};
    paddr_t    physical{};
    page_size  size{};
    page_flags flags{};
};

// =================================================================================================
// Page table
// =================================================================================================

class alignas(PAGE_SIZE_4K) page_table {
    u64 entries_m[ENTRIES_PER_TABLE]{};

public:
    void clear();

    [[nodiscard]] u64 &operator[](usize index)
    {
        return entries_m[index];
    }

    [[nodiscard]] const u64 &operator[](usize index) const
    {
        return entries_m[index];
    }
};

static_assert(sizeof(page_table) == PAGE_SIZE_4K);
static_assert(alignof(page_table) == PAGE_SIZE_4K);

// =================================================================================================
// Address space
// =================================================================================================

class address_space {
    paddr_t root_m{};

public:
    constexpr address_space() = default;

    explicit constexpr address_space(paddr_t root)
        : root_m(root)
    {
    }

    [[nodiscard]] paddr_t root() const
    {
        return root_m;
    }

    [[nodiscard]] bool valid() const
    {
        return root_m != 0;
    }

    void set_root(paddr_t root)
    {
        root_m = root;
    }

    [[nodiscard]] bool map(vaddr_t virtual_address, paddr_t physical_address, page_size size,
                           page_flags flags);
    [[nodiscard]] bool unmap(vaddr_t virtual_address, page_size size);
    [[nodiscard]] mapping translate(vaddr_t virtual_address) const;
};

}  // namespace kernel::core::memory::vmm::mmu

#endif  // DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_PAGETABLE_HPP_
