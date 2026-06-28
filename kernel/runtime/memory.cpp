// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/runtime/memory.hpp"

namespace kernel::runtime {

using kernel::core::i32;
using kernel::core::u8;
using kernel::core::usize;

// =================================================================================================
// Memory functionality
// =================================================================================================

void *set(void *dest, i32 value, usize count) noexcept {
    auto *d = static_cast<u8 *>(dest);
    auto  v = static_cast<u8>(value);

    for (usize i = 0; i < count; ++i) d[i] = v;

    return dest;
}

void *copy(void *dest, const void *src, usize count) noexcept {
    auto       *d = static_cast<u8 *>(dest);
    const auto *s = static_cast<const u8 *>(src);

    for (usize i = 0; i < count; ++i) d[i] = s[i];

    return dest;
}

void *move(void *dest, const void *src, usize count) noexcept {
    auto       *d = static_cast<u8 *>(dest);
    const auto *s = static_cast<const u8 *>(src);

    if (d == s || count == 0)
        return dest;

    const auto d_addr = reinterpret_cast<usize>(d);
    const auto s_addr = reinterpret_cast<usize>(s);

    if (d_addr < s_addr) {
        for (usize i = 0; i < count; ++i) d[i] = s[i];
    } else {
        for (usize i = count; i > 0; --i) d[i - 1] = s[i - 1];
    }

    return dest;
}

i32 compare(const void *lhs, const void *rhs, usize count) noexcept {
    const auto *a = static_cast<const u8 *>(lhs);
    const auto *b = static_cast<const u8 *>(rhs);

    for (usize i = 0; i < count; ++i) {
        if (a[i] != b[i])
            return static_cast<i32>(a[i]) - static_cast<i32>(b[i]);
    }

    return 0;
}

}  // namespace kernel::runtime

// =================================================================================================
// C ABI wrappers
// =================================================================================================

extern "C" void *memset(void *dest, kernel::core::i32 value, kernel::core::usize count) noexcept {
    return kernel::runtime::set(dest, value, count);
}

extern "C" void *memcpy(void *dest, const void *src, kernel::core::usize count) noexcept {
    return kernel::runtime::copy(dest, src, count);
}

extern "C" void *memmove(void *dest, const void *src, kernel::core::usize count) noexcept {
    return kernel::runtime::move(dest, src, count);
}

extern "C" kernel::core::i32 memcmp(const void *lhs, const void *rhs,
                                    kernel::core::usize count) noexcept {
    return kernel::runtime::compare(lhs, rhs, count);
}