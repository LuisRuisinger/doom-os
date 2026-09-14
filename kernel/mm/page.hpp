#ifndef DOOM_OS_KERNEL_MM_PAGE_HPP_
#define DOOM_OS_KERNEL_MM_PAGE_HPP_

#include "kernel/core/cast.hpp"
#include "kernel/core/types.hpp"

namespace kernel::mm {

using kernel::core::paddr_t;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

enum class page_size : u8 {
    SIZE_4K = 12,
    SIZE_2M = 21,
    SIZE_1G = 30,
};

inline constexpr u64 FRAME_SHIFT = page_size::SIZE_4K as(u64);
inline constexpr u64                                  FRAME_SIZE = u64{1} << FRAME_SHIFT;

[[nodiscard]] constexpr u64 shift_of(page_size size)
{
    return size as(u64);
}

[[nodiscard]] constexpr u64 bytes_in(page_size size)
{
    return u64{1} << shift_of(size);
}

[[nodiscard]] constexpr usize frames_in(page_size size)
{
    return usize{1} << (shift_of(size) - FRAME_SHIFT);
}

enum class page_prot : u8 {
    NONE = 0,
    WRITE = 1 << 0,
    EXEC = 1 << 1,
    USER = 1 << 2,
    GLOBAL = 1 << 3,
    UNCACHED = 1 << 4,

    KERNEL_TEXT = EXEC | GLOBAL,
    KERNEL_RODATA = GLOBAL,
    KERNEL_DATA = WRITE | GLOBAL,
    MMIO = WRITE | GLOBAL | UNCACHED,
};

[[nodiscard]] constexpr page_prot operator|(page_prot a, page_prot b)
{
    return (a as(u8) | b as(u8)) as(page_prot);
}

[[nodiscard]] constexpr page_prot operator&(page_prot a, page_prot b)
{
    return (a as(u8) & b as(u8)) as(page_prot);
}

constexpr page_prot &operator|=(page_prot &a, page_prot b)
{
    return a = a | b;
}

[[nodiscard]] constexpr bool has(page_prot set, page_prot bit)
{
    return (set & bit) != page_prot::NONE;
}

enum class mm_error : u8 {
    OUT_OF_MEMORY,
    INVALID_ADDRESS,
    UNSUPPORTED_SIZE,
    NOT_MAPPED,
    CONFLICT,
};

struct mapping {
    paddr_t   frame;
    u64       offset;
    page_size size;
    page_prot prot;

    [[nodiscard]] paddr_t physical() const
    {
        return frame + offset;
    }
};

}  // namespace kernel::mm

#endif  // DOOM_OS_KERNEL_MM_PAGE_HPP_
