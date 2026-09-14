
#include "kernel/runtime/memory.hpp"

#include "kernel/core/cast.hpp"
#include "kernel/core/types.hpp"

namespace {

using kernel::core::i32;
using kernel::core::u8;
using kernel::core::usize;

}  // namespace

extern "C" void *memset(void *dest, i32 value, usize count) noexcept
{
    auto *d = dest as(u8 *);
    auto v = value as(u8);

    for (usize i = 0; i < count; ++i)
        d[i] = v;

    return dest;
}

extern "C" void *memcpy(void *dest, const void *src, usize count) noexcept
{
    auto       *d = dest as(u8 *);
    const auto *s = src as(const u8 *);

    for (usize i = 0; i < count; ++i)
        d[i] = s[i];

    return dest;
}

extern "C" void *memmove(void *dest, const void *src, usize count) noexcept
{
    auto       *d = dest as(u8 *);
    const auto *s = src as(const u8 *);

    if (d == s || count == 0)
        return dest;

    const auto d_addr = d as(usize);
    const auto s_addr = s as(usize);

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
    const auto *a = lhs as(const u8 *);
    const auto *b = rhs as(const u8 *);

    for (usize i = 0; i < count; ++i) {
        if (a[i] != b[i])
            return a[i] as(i32) - b[i] as(i32);
    }

    return 0;
}
