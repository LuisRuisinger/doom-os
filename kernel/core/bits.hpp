#ifndef DOOM_OS_KERNEL_CORE_BITS_HPP_
#define DOOM_OS_KERNEL_CORE_BITS_HPP_

#include "kernel/core/cast.hpp"
#include "kernel/core/types.hpp"

namespace kernel::core::utils {

template <typename T>
constexpr __attribute__((always_inline)) u32 bit_width()
{
    return (sizeof(T) * __CHAR_BIT__) as(u32);
}

template <typename T>
constexpr __attribute__((always_inline)) T all_bits()
{
    return (~T{0}) as(T);
}

template <typename T = u32>
constexpr __attribute__((always_inline)) T bit(u32 index)
{
    return index >= bit_width<T>() ? T{0} : (T{1} << index) as(T);
}

template <typename T>
constexpr __attribute__((always_inline)) bool test_bit(T value, u32 index)
{
    return index < bit_width<T>() && (value & bit<T>(index)) != T{0};
}

template <typename T>
constexpr __attribute__((always_inline)) T set_bit(T value, u32 index)
{
    return index >= bit_width<T>() ? value : (value | bit<T>(index)) as(T);
}

template <typename T>
constexpr __attribute__((always_inline)) T clear_bit(T value, u32 index)
{
    return index >= bit_width<T>() ? value : (value & ~bit<T>(index)) as(T);
}

template <typename T = u32>
constexpr __attribute__((always_inline)) T mask(u32 bit_count)
{
    return bit_count >= bit_width<T>() ? all_bits<T>() : (bit<T>(bit_count) - T{1}) as(T);
}

template <typename T = usize>
constexpr __attribute__((always_inline)) bool is_power_of_two(T value)
{
    return value != T{0} && (value & (value - T{1}) as(T)) == T{0};
}

template <typename T = usize>
constexpr __attribute__((always_inline)) T align_down(T value, T alignment)
{
    return (value & ~(alignment - T{1})) as(T);
}

template <typename T = usize>
constexpr __attribute__((always_inline)) T align_up(T value, T alignment)
{
    return align_down<T>((value + alignment - T{1}) as(T), alignment);
}

template <typename T = usize>
constexpr __attribute__((always_inline)) bool is_aligned(T value, T alignment)
{
    return (value & (alignment - T{1}) as(T)) == T{0};
}

static_assert(bit_width<u8>() == 8);
static_assert(bit_width<u16>() == 16);
static_assert(bit_width<u32>() == 32);
static_assert(bit_width<u64>() == 64);

static_assert(bit(0) == u32{0x1});
static_assert(bit(1) == u32{0x2});
static_assert(bit(2) == u32{0x4});
static_assert(bit(3) == u32{0x8});
static_assert(bit(12) == u32{0x1000});
static_assert(bit(31) == u32{0x80000000});
static_assert(bit(32) == u32{0});

static_assert(bit<u64>(0) == u64{0x1});
static_assert(bit<u64>(1) == u64{0x2});
static_assert(bit<u64>(12) == u64{0x1000});
static_assert(bit<u64>(31) == u64{0x80000000});
static_assert(bit<u64>(32) == u64{0x100000000});
static_assert(bit<u64>(63) == u64{0x8000000000000000ULL});
static_assert(bit<u64>(64) == u64{0});

static_assert(mask(0) == u32{0x0});
static_assert(mask(1) == u32{0x1});
static_assert(mask(2) == u32{0x3});
static_assert(mask(3) == u32{0x7});
static_assert(mask(4) == u32{0xf});
static_assert(mask(8) == u32{0xff});
static_assert(mask(12) == u32{0xfff});
static_assert(mask(16) == u32{0xffff});
static_assert(mask(31) == u32{0x7fffffff});
static_assert(mask(32) == u32{0xffffffff});
static_assert(mask(33) == u32{0xffffffff});

static_assert(mask<u64>(0) == u64{0x0});
static_assert(mask<u64>(1) == u64{0x1});
static_assert(mask<u64>(2) == u64{0x3});
static_assert(mask<u64>(3) == u64{0x7});
static_assert(mask<u64>(4) == u64{0xf});
static_assert(mask<u64>(8) == u64{0xff});
static_assert(mask<u64>(12) == u64{0xfff});
static_assert(mask<u64>(16) == u64{0xffff});
static_assert(mask<u64>(31) == u64{0x7fffffff});
static_assert(mask<u64>(32) == u64{0xffffffff});
static_assert(mask<u64>(63) == u64{0x7fffffffffffffffULL});
static_assert(mask<u64>(64) == u64{0xffffffffffffffffULL});
static_assert(mask<u64>(65) == u64{0xffffffffffffffffULL});

static_assert(test_bit(u32{0b0001}, 0));
static_assert(!test_bit(u32{0b0001}, 1));
static_assert(test_bit(u32{0b0010}, 1));
static_assert(test_bit(u32{0b1010}, 1));
static_assert(!test_bit(u32{0b1010}, 2));
static_assert(test_bit(u32{0b1010}, 3));
static_assert(!test_bit(u32{0b1010}, 32));

static_assert(test_bit(u64{0x8000000000000000ULL}, 63));
static_assert(!test_bit(u64{0x8000000000000000ULL}, 64));

static_assert(set_bit(u32{0}, 0) == u32{0b0001});
static_assert(set_bit(u32{0}, 1) == u32{0b0010});
static_assert(set_bit(u32{0}, 3) == u32{0b1000});
static_assert(set_bit(u32{0b0010}, 1) == u32{0b0010});
static_assert(set_bit(u32{0b0010}, 2) == u32{0b0110});
static_assert(set_bit(u32{0}, 32) == u32{0});

static_assert(set_bit(u64{0}, 63) == u64{0x8000000000000000ULL});
static_assert(set_bit(u64{0}, 64) == u64{0});

static_assert(clear_bit(u32{0b0001}, 0) == u32{0b0000});
static_assert(clear_bit(u32{0b0011}, 0) == u32{0b0010});
static_assert(clear_bit(u32{0b1010}, 1) == u32{0b1000});
static_assert(clear_bit(u32{0b1010}, 2) == u32{0b1010});
static_assert(clear_bit(u32{0b1010}, 3) == u32{0b0010});
static_assert(clear_bit(u32{0xffffffff}, 31) == u32{0x7fffffff});
static_assert(clear_bit(u32{0xffffffff}, 32) == u32{0xffffffff});

static_assert(clear_bit(u64{0xffffffffffffffffULL}, 63) == u64{0x7fffffffffffffffULL});
static_assert(clear_bit(u64{0xffffffffffffffffULL}, 64) == u64{0xffffffffffffffffULL});

static_assert(is_power_of_two(usize{1}));
static_assert(is_power_of_two(usize{2}));
static_assert(is_power_of_two(usize{4}));
static_assert(is_power_of_two(usize{8}));
static_assert(is_power_of_two(usize{4096}));
static_assert(is_power_of_two(u64{0x8000000000000000ULL}));

static_assert(!is_power_of_two(usize{0}));
static_assert(!is_power_of_two(usize{3}));
static_assert(!is_power_of_two(usize{5}));
static_assert(!is_power_of_two(usize{7}));
static_assert(!is_power_of_two(usize{4097}));

static_assert(align_down(usize{0}, usize{8}) == usize{0});
static_assert(align_down(usize{1}, usize{8}) == usize{0});
static_assert(align_down(usize{7}, usize{8}) == usize{0});
static_assert(align_down(usize{8}, usize{8}) == usize{8});
static_assert(align_down(usize{9}, usize{8}) == usize{8});
static_assert(align_down(usize{15}, usize{8}) == usize{8});
static_assert(align_down(usize{16}, usize{8}) == usize{16});
static_assert(align_down(usize{0x1003}, usize{0x1000}) == usize{0x1000});
static_assert(align_down(usize{0x1fff}, usize{0x1000}) == usize{0x1000});
static_assert(align_down(usize{0x2000}, usize{0x1000}) == usize{0x2000});

static_assert(align_up(usize{0}, usize{8}) == usize{0});
static_assert(align_up(usize{1}, usize{8}) == usize{8});
static_assert(align_up(usize{7}, usize{8}) == usize{8});
static_assert(align_up(usize{8}, usize{8}) == usize{8});
static_assert(align_up(usize{9}, usize{8}) == usize{16});
static_assert(align_up(usize{15}, usize{8}) == usize{16});
static_assert(align_up(usize{16}, usize{8}) == usize{16});
static_assert(align_up(usize{0x1003}, usize{0x1000}) == usize{0x2000});
static_assert(align_up(usize{0x1fff}, usize{0x1000}) == usize{0x2000});
static_assert(align_up(usize{0x2000}, usize{0x1000}) == usize{0x2000});

static_assert(is_aligned(usize{0}, usize{8}));
static_assert(!is_aligned(usize{1}, usize{8}));
static_assert(!is_aligned(usize{7}, usize{8}));
static_assert(is_aligned(usize{8}, usize{8}));
static_assert(!is_aligned(usize{9}, usize{8}));
static_assert(is_aligned(usize{16}, usize{8}));
static_assert(is_aligned(usize{0x1000}, usize{0x1000}));
static_assert(!is_aligned(usize{0x1001}, usize{0x1000}));
static_assert(is_aligned(usize{0x2000}, usize{0x1000}));

}  // namespace kernel::core::utils

#endif  // DOOM_OS_KERNEL_CORE_BITS_HPP_