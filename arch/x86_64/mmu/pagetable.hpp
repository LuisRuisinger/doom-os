#ifndef DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_PAGETABLE_HPP_
#define DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_PAGETABLE_HPP_

// =================================================================================================
// Config files
// =================================================================================================

#include "config/layout.h"

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/bitmap.hpp"
#include "kernel/core/memory/page.hpp"
#include "kernel/core/types.hpp"

namespace kernel::core::memory::vmm::mmu {

using kernel::core::paddr_t;
using kernel::core::u64;
using kernel::core::usize;
using kernel::core::vaddr_t;
using kernel::core::memory::bytes_in;
using kernel::core::memory::is_hw_allocatable;
using kernel::core::memory::page;
using kernel::core::memory::page_1g;
using kernel::core::memory::page_2m;
using kernel::core::memory::page_4k;
using kernel::core::memory::page_hw_allocatability;
using kernel::core::memory::page_size;
using kernel::core::memory::PAGE_SHIFT_1G;
using kernel::core::memory::PAGE_SHIFT_2M;
using kernel::core::memory::PAGE_SHIFT_4K;
using kernel::core::memory::PAGE_SIZE_1G;
using kernel::core::memory::PAGE_SIZE_2M;
using kernel::core::memory::PAGE_SIZE_4K;

// =================================================================================================
// Paging constants
// =================================================================================================

inline constexpr usize ENTRIES_PER_TABLE = 512;

inline constexpr u64 PML4_SLOT_SIZE = u64{1} << 39;

inline constexpr vaddr_t KERNEL_BASE = static_cast<vaddr_t>(DOOM_OS_KERNEL_VMA);
inline constexpr vaddr_t DIRECT_MAP_BASE = static_cast<vaddr_t>(DOOM_OS_DIRECT_MAP_BASE);
inline constexpr u64     DIRECT_MAP_SIZE = static_cast<u64>(DOOM_OS_MAX_PHYSICAL_MEMORY);

inline constexpr u64 EARLY_MAP_SIZE = static_cast<u64>(DOOM_OS_EARLY_MAP_SIZE);

static_assert((KERNEL_BASE & (PAGE_SIZE_1G - 1)) == 0, "kernel base must be 1 GiB aligned");
static_assert((DIRECT_MAP_BASE & (PML4_SLOT_SIZE - 1)) == 0,
              "direct map base must be 512 GiB aligned");
static_assert(DIRECT_MAP_SIZE % PAGE_SIZE_2M == 0,
              "direct map size must be expressible as 2 MiB leaves");

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

using page_flags = kernel::core::utils::bitmap<64, u64>;

inline constexpr u64 PAGE_FLAG_WRITABLE = u64{1} << 1;
inline constexpr u64 PAGE_FLAG_USER = u64{1} << 2;
inline constexpr u64 PAGE_FLAG_WRITE_THROUGH = u64{1} << 3;
inline constexpr u64 PAGE_FLAG_CACHE_DISABLE = u64{1} << 4;
inline constexpr u64 PAGE_FLAG_GLOBAL = u64{1} << 8;
inline constexpr u64 PAGE_FLAG_NO_EXECUTE = u64{1} << 63;
inline constexpr u64 PAGE_FLAGS_MASK = PAGE_FLAG_WRITABLE | PAGE_FLAG_USER |
                                       PAGE_FLAG_WRITE_THROUGH | PAGE_FLAG_CACHE_DISABLE |
                                       PAGE_FLAG_GLOBAL | PAGE_FLAG_NO_EXECUTE;

// A resolved translation. frame and offset are kept apart because they answer different
// questions: a caller re-mapping or freeing the page wants the frame, one chasing a pointer
// wants the exact address. Fusing them into one field meant every frame user had to mask the
// offset back out using size, and getting that wrong is silent.
struct mapping {
    bool       present{};
    paddr_t    frame{};
    u64        offset{};
    page_size  size{};
    page_flags flags{};

    [[nodiscard]] paddr_t physical() const
    {
        return frame + offset;
    }
};

// =================================================================================================
// Reserved tables
//
// The bootstrap tables are static objects inside the kernel image rather than PMM frames, so an
// emptied one must never be handed to the frame allocator - that is a free of memory the PMM has
// marked reserved, which panics. The bootstrap registers the image's physical range here so the
// walker can tell the two kinds of table apart when it collects empty ones.
// =================================================================================================

void set_reserved_table_range(paddr_t first, paddr_t last);

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

    // Establishes a mapping, replacing any existing one of the same size at this address. A
    // mapping of a *different* size is refused rather than torn down, because deciding that the
    // caller meant to drop the other 511 pages of a large page is not this layer's call.
    [[nodiscard]] bool map(vaddr_t virtual_address, paddr_t physical_address, page_size size,
                           page_flags flags);

    // Rewrites the permissions of an existing leaf, leaving the frame alone. What a caller
    // hardening an already-mapped region actually means - map() would force it to restate the
    // physical address it is deliberately not changing.
    [[nodiscard]] bool protect(vaddr_t virtual_address, page_size size, page_flags flags);

    [[nodiscard]] bool unmap(vaddr_t virtual_address, page_size size);
    [[nodiscard]] mapping translate(vaddr_t virtual_address) const;
};

}  // namespace kernel::core::memory::vmm::mmu

#endif  // DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_PAGETABLE_HPP_
