// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/runtime/memory.hpp"

namespace {

using kernel::core::i32;
using kernel::core::u8;
using kernel::core::usize;

}  // namespace

// =================================================================================================
// Freestanding libc memory functions
//
// GCC emits calls to these regardless of -ffreestanding - a struct copy or a zero-initialised
// array is enough - so they have to exist as global C symbols with exactly these names.
// =================================================================================================

extern "C" void *memset(void *dest, i32 value, usize count) noexcept
{
    auto *d = static_cast<u8 *>(dest);
    auto  v = static_cast<u8>(value);

    for (usize i = 0; i < count; ++i)
        d[i] = v;

    return dest;
}

extern "C" void *memcpy(void *dest, const void *src, usize count) noexcept
{
    auto       *d = static_cast<u8 *>(dest);
    const auto *s = static_cast<const u8 *>(src);

    for (usize i = 0; i < count; ++i)
        d[i] = s[i];

    return dest;
}

extern "C" void *memmove(void *dest, const void *src, usize count) noexcept
{
    auto       *d = static_cast<u8 *>(dest);
    const auto *s = static_cast<const u8 *>(src);

    if (d == s || count == 0)
        return dest;

    const auto d_addr = reinterpret_cast<usize>(d);
    const auto s_addr = reinterpret_cast<usize>(s);

    if (d_addr < s_addr) {
        for (usize i = 0; i < count; ++i)
            d[i] = s[i];
    } else {
        for (usize i = count; i > 0; --i)
            d[i - 1] = s[i - 1];
    }

    return dest;
}

extern "C" i32 memcmp(const void *lhs, const void *rhs, usize count) noexcept
{
    const auto *a = static_cast<const u8 *>(lhs);
    const auto *b = static_cast<const u8 *>(rhs);

    for (usize i = 0; i < count; ++i) {
        if (a[i] != b[i])
            return static_cast<i32>(a[i]) - static_cast<i32>(b[i]);
    }

    return 0;
}
