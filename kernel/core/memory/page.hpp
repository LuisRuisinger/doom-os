#ifndef DOOM_OS_KERNEL_CORE_MEMORY_PAGE_HPP_
#define DOOM_OS_KERNEL_CORE_MEMORY_PAGE_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"

namespace kernel::core::memory {

using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

// =================================================================================================
// Page geometry
//
// Shared page-size facts. A layer can allocate or map only the sizes it explicitly supports, but
// the byte counts, shifts and 4 KiB frame counts should have one source of truth.
// =================================================================================================

enum class page_size : u8 {
    SIZE_4K,
    SIZE_2M,
    SIZE_1G,
};

enum class page_hw_allocatability : u8 {
    UNINITIALIZED,
    NO,
    YES,
};

template <usize Bytes>
struct page {
    static constexpr u64   bytes = static_cast<u64>(Bytes);
    static constexpr u64   shift = 0;
    static constexpr usize frames = 0;
    static constexpr bool  is_sw_allocatable = false;

    // Hardware-allocatable means representable as a hardware translation leaf. The CPU does not
    // allocate physical memory; PMM does.
    [[nodiscard]] static constexpr page_hw_allocatability is_hw_allocatable()
    {
        return page_hw_allocatability::UNINITIALIZED;
    }
};

template <>
struct page<4096> {
    static constexpr u64   bytes = 4096;
    static constexpr u64   shift = 12;
    static constexpr usize frames = 1;
    static constexpr bool  is_sw_allocatable = true;

    [[nodiscard]] static constexpr page_hw_allocatability is_hw_allocatable()
    {
        return page_hw_allocatability::YES;
    }
};

template <>
struct page<2 * 1024 * 1024> {
    static constexpr u64   bytes = 2 * 1024 * 1024;
    static constexpr u64   shift = 21;
    static constexpr usize frames = 512;
    static constexpr bool  is_sw_allocatable = true;

    [[nodiscard]] static constexpr page_hw_allocatability is_hw_allocatable()
    {
        return page_hw_allocatability::YES;
    }
};

template <>
struct page<1024 * 1024 * 1024> {
    static constexpr u64   bytes = 1024 * 1024 * 1024;
    static constexpr u64   shift = 30;
    static constexpr usize frames = 512 * 512;
    static constexpr bool  is_sw_allocatable = true;

    [[nodiscard]] static page_hw_allocatability is_hw_allocatable();
};

using page_4k = page<4096>;
using page_2m = page<2 * 1024 * 1024>;
using page_1g = page<1024 * 1024 * 1024>;

inline constexpr u64 PAGE_SHIFT_4K = page_4k::shift;
inline constexpr u64 PAGE_SHIFT_2M = page_2m::shift;
inline constexpr u64 PAGE_SHIFT_1G = page_1g::shift;

inline constexpr u64 PAGE_SIZE_4K = page_4k::bytes;
inline constexpr u64 PAGE_SIZE_2M = page_2m::bytes;
inline constexpr u64 PAGE_SIZE_1G = page_1g::bytes;

inline constexpr u64   FRAME_SIZE = PAGE_SIZE_4K;
inline constexpr u64   FRAME_SHIFT = PAGE_SHIFT_4K;
inline constexpr usize FRAMES_PER_2M = page_2m::frames;
inline constexpr usize FRAMES_PER_1G = page_1g::frames;

[[nodiscard]] inline constexpr u64 bytes_in(page_size size)
{
    switch (size) {
        case page_size::SIZE_4K:
            return page_4k::bytes;
        case page_size::SIZE_2M:
            return page_2m::bytes;
        case page_size::SIZE_1G:
            return page_1g::bytes;
    }

    return 0;
}

[[nodiscard]] inline constexpr u64 shift_of(page_size size)
{
    switch (size) {
        case page_size::SIZE_4K:
            return page_4k::shift;
        case page_size::SIZE_2M:
            return page_2m::shift;
        case page_size::SIZE_1G:
            return page_1g::shift;
    }

    return 0;
}

[[nodiscard]] inline constexpr usize frames_in(page_size size)
{
    switch (size) {
        case page_size::SIZE_4K:
            return page_4k::frames;
        case page_size::SIZE_2M:
            return page_2m::frames;
        case page_size::SIZE_1G:
            return page_1g::frames;
    }

    return 0;
}

[[nodiscard]] inline constexpr bool is_sw_allocatable(page_size size)
{
    switch (size) {
        case page_size::SIZE_4K:
            return page_4k::is_sw_allocatable;
        case page_size::SIZE_2M:
            return page_2m::is_sw_allocatable;
        case page_size::SIZE_1G:
            return page_1g::is_sw_allocatable;
    }

    return false;
}

[[nodiscard]] page_hw_allocatability is_hw_allocatable(page_size size);
void set_hw_allocatable(page_size size, bool allocatable);

[[nodiscard]] inline constexpr bool is_page_size(u64 bytes)
{
    return bytes == page_4k::bytes || bytes == page_2m::bytes || bytes == page_1g::bytes;
}

[[nodiscard]] inline constexpr page_size page_size_of(u64 bytes)
{
    return bytes == page_4k::bytes   ? page_size::SIZE_4K
           : bytes == page_2m::bytes ? page_size::SIZE_2M
                                     : page_size::SIZE_1G;
}

}  // namespace kernel::core::memory

#endif  // DOOM_OS_KERNEL_CORE_MEMORY_PAGE_HPP_
