#ifndef DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_ENTRY_HPP_
#define DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_ENTRY_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/memory/vmm/mmu/features.hpp"
#include "kernel/core/memory/vmm/mmu/pagetable.hpp"

namespace kernel::core::memory::vmm::mmu {

// =================================================================================================
// x86_64 page-table entries
//
// Internal to the MMU implementation. This is the only place that knows how a page_flags becomes
// a hardware entry and back; both the page-table walker and the bootstrap builder encode through
// here, because two copies of this that drift are two different meanings of the same mapping.
// =================================================================================================

inline constexpr u64 ENTRY_PRESENT = u64{1} << 0;
inline constexpr u64 ENTRY_WRITE = u64{1} << 1;
inline constexpr u64 ENTRY_USER = u64{1} << 2;
inline constexpr u64 ENTRY_WRITE_THROUGH = u64{1} << 3;
inline constexpr u64 ENTRY_CACHE_DISABLE = u64{1} << 4;
inline constexpr u64 ENTRY_LARGE = u64{1} << 7;
inline constexpr u64 ENTRY_GLOBAL = u64{1} << 8;
inline constexpr u64 ENTRY_NO_EXECUTE = u64{1} << 63;

inline constexpr u64 ENTRY_ADDRESS_MASK = 0x000FFFFFFFFFF000ULL;

// The bits a split carries down to the children. ENTRY_LARGE is deliberately absent: it means
// something different at every level, so the caller re-adds it for the level it is writing.
inline constexpr u64 ENTRY_INHERITED_FLAGS = ENTRY_PRESENT | ENTRY_WRITE | ENTRY_USER |
                                             ENTRY_WRITE_THROUGH | ENTRY_CACHE_DISABLE |
                                             ENTRY_GLOBAL | ENTRY_NO_EXECUTE;

[[nodiscard]] inline bool present(u64 entry)
{
    return (entry & ENTRY_PRESENT) != 0;
}

[[nodiscard]] inline bool large(u64 entry)
{
    return (entry & ENTRY_LARGE) != 0;
}

[[nodiscard]] inline paddr_t entry_address(u64 entry)
{
    return static_cast<paddr_t>(entry & ENTRY_ADDRESS_MASK);
}

[[nodiscard]] inline page_flags flags_from_entry(u64 entry)
{
    return page_flags{
        .writable = (entry & ENTRY_WRITE) != 0,
        .executable = (entry & ENTRY_NO_EXECUTE) == 0,
        .user = (entry & ENTRY_USER) != 0,
        .global = (entry & ENTRY_GLOBAL) != 0,
        .write_through = (entry & ENTRY_WRITE_THROUGH) != 0,
        .cache_disable = (entry & ENTRY_CACHE_DISABLE) != 0,
    };
}

// NX and GLOBAL are dropped when the CPU is not running with them enabled: an entry carrying a
// bit the control registers have not turned on is a reserved-bit fault, not a stricter mapping.
[[nodiscard]] inline u64 entry_flags(page_flags flags)
{
    u64 entry = ENTRY_PRESENT;

    if (flags.writable)
        entry |= ENTRY_WRITE;

    if (flags.user)
        entry |= ENTRY_USER;

    if (flags.write_through)
        entry |= ENTRY_WRITE_THROUGH;

    if (flags.cache_disable)
        entry |= ENTRY_CACHE_DISABLE;

    if (flags.global && features().global)
        entry |= ENTRY_GLOBAL;

    if (!flags.executable && features().nx)
        entry |= ENTRY_NO_EXECUTE;

    return entry;
}

// Intermediate entries stay permissive; the leaf is what decides access. Marking a table
// read-only or non-executable would mask every leaf below it.
[[nodiscard]] inline u64 table_entry_flags(bool user)
{
    return ENTRY_PRESENT | ENTRY_WRITE | (user ? ENTRY_USER : 0);
}

[[nodiscard]] inline u64 table_entry(paddr_t physical, bool user)
{
    return (physical & ENTRY_ADDRESS_MASK) | table_entry_flags(user);
}

[[nodiscard]] inline u64 leaf_entry(paddr_t physical, page_size size, page_flags flags)
{
    return (physical & ENTRY_ADDRESS_MASK) | entry_flags(flags) |
           (size == page_size::SIZE_4K ? 0 : ENTRY_LARGE);
}

}  // namespace kernel::core::memory::vmm::mmu

#endif  // DOOM_OS_KERNEL_CORE_MEMORY_VMM_MMU_ENTRY_HPP_
