#ifndef DOOM_OS_ARCH_X86_64_MMU_ENTRY_HPP_
#define DOOM_OS_ARCH_X86_64_MMU_ENTRY_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "arch/x86_64/mmu/features.hpp"
#include "arch/x86_64/mmu/pagetable.hpp"

namespace kernel::arch::x86_64::mmu {

// =================================================================================================
// x86_64 page-table entries
//
// Internal to the MMU implementation. This is the only place that knows how a page_flags becomes
// a hardware entry and back; both the page-table walker and the bootstrap builder encode through
// here, because two copies of this that drift are two different meanings of the same mapping.
// =================================================================================================

inline constexpr u64 ENTRY_PRESENT = u64{1} << 0;
inline constexpr u64 ENTRY_LARGE = u64{1} << 7;

inline constexpr u64 ENTRY_ADDRESS_MASK = 0x000FFFFFFFFFF000ULL;

// The bits a split carries down to the children. ENTRY_LARGE is deliberately absent: it means
// something different at every level, so the caller re-adds it for the level it is writing.
inline constexpr u64 ENTRY_INHERITED_FLAGS = ENTRY_PRESENT | PAGE_FLAGS_MASK;

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
    page_flags flags{};

    flags.word(0) = entry & PAGE_FLAGS_MASK;

    return flags;
}

// NX and GLOBAL are dropped when the CPU is not running with them enabled: an entry carrying a
// bit the control registers have not turned on is a reserved-bit fault, not a stricter mapping.
[[nodiscard]] inline u64 entry_flags(page_flags flags)
{
    u64 entry = ENTRY_PRESENT;
    const u64 raw_flags = flags.word(0);

    entry |= raw_flags & (PAGE_FLAG_WRITABLE | PAGE_FLAG_USER | PAGE_FLAG_WRITE_THROUGH |
                          PAGE_FLAG_CACHE_DISABLE);

    if ((raw_flags & PAGE_FLAG_GLOBAL) != 0 && global_pages_enabled())
        entry |= PAGE_FLAG_GLOBAL;

    if ((raw_flags & PAGE_FLAG_NO_EXECUTE) != 0 && nx_enabled())
        entry |= PAGE_FLAG_NO_EXECUTE;

    return entry;
}

// Intermediate entries stay permissive; the leaf is what decides access. Marking a table
// read-only or non-executable would mask every leaf below it.
[[nodiscard]] inline u64 table_entry_flags(bool user)
{
    return ENTRY_PRESENT | PAGE_FLAG_WRITABLE | (user ? PAGE_FLAG_USER : 0);
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

}  // namespace kernel::arch::x86_64::mmu

#endif  // DOOM_OS_ARCH_X86_64_MMU_ENTRY_HPP_
