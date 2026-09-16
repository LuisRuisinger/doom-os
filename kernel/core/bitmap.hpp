#ifndef DOOM_OS_KERNEL_CORE_BITMAP_HPP_
#define DOOM_OS_KERNEL_CORE_BITMAP_HPP_

#include <bit>
#include <limits>
#include <type_traits>

#include "kernel/core/array.hpp"
#include "kernel/core/bits.hpp"
#include "kernel/core/cast.hpp"
#include "kernel/core/types.hpp"

namespace kernel::core::utils {

template <usize N, typename StorageType = u64>
class bitmap {
public:
    static_assert(std::is_unsigned_v<StorageType>, "bitmap word type must be unsigned");
    static_assert(N > 0, "a bitmap cannot be empty");

    static constexpr usize BITS_PER_STORAGE_TYPE_INSTANCE = utils::bit_width<StorageType>();
    static constexpr usize STORAGE_TYPE_INSTANCE_COUNT = N / BITS_PER_STORAGE_TYPE_INSTANCE;
    static constexpr usize NPOS = std::numeric_limits<usize>::max();

private:
    [[nodiscard]] static constexpr __attribute__((always_inline)) usize word_of(usize index)
    {
        return index / BITS_PER_STORAGE_TYPE_INSTANCE;
    }

    [[nodiscard]] static constexpr __attribute__((always_inline)) u32 offset_of(usize index)
    {
        return (index % BITS_PER_STORAGE_TYPE_INSTANCE) as(u32);
    }

    [[nodiscard]] static constexpr __attribute__((always_inline)) StorageType bit_of(usize index)
    {
        return StorageType{1} << offset_of(index);
    }

    [[nodiscard]] static constexpr __attribute__((always_inline)) StorageType mask_from(usize first)
    {
        return ~mask<StorageType>(offset_of(first));
    }

    [[nodiscard]] static constexpr __attribute__((always_inline)) StorageType mask_upto(usize last)
    {
        return mask<StorageType>(offset_of(last) + 1);
    }

    [[nodiscard]] static constexpr __attribute__((always_inline)) auto countr_zero(
        StorageType value)
    {
        return std::countr_zero(value);
    }

    [[nodiscard]] static constexpr __attribute__((always_inline)) auto popcount(StorageType value)
    {
        return std::popcount(value);
    }

    constexpr __attribute__((always_inline)) void apply(usize first, usize count, bool value)
    {
        if (count == 0)
            return;

        const usize last = first + count - 1;
        const usize first_word = word_of(first);
        const usize last_word = word_of(last);

        if (first_word == last_word) {
            apply_word(first_word, mask_from(first) & mask_upto(last), value);

            return;
        }

        apply_word(first_word, mask_from(first), value);
        for (usize word = first_word + 1; word < last_word; ++word)
            m_words[word] = value ? std::numeric_limits<StorageType>::max() : StorageType{0};

        apply_word(last_word, mask_upto(last), value);
    }

    constexpr __attribute__((always_inline)) void apply_word(usize word, StorageType selected,
                                                             bool value)
    {
        if (value) {
            m_words[word] |= selected;
        } else {
            m_words[word] &= ~selected;
        }
    }

    constexpr void shift_left(usize count)
    {
        if (count == 0)
            return;

        if (count >= N) {
            reset();
            return;
        }

        const usize word_shift = count / BITS_PER_STORAGE_TYPE_INSTANCE;
        const usize bit_shift = count % BITS_PER_STORAGE_TYPE_INSTANCE;

        for (usize destination = STORAGE_TYPE_INSTANCE_COUNT; destination-- > 0;) {
            if (destination < word_shift) {
                m_words[destination] = 0;
                continue;
            }

            const usize source = destination - word_shift;

            StorageType value = m_words[source] << bit_shift;
            if (bit_shift != 0 && source != 0)
                value |= m_words[source - 1] >> (BITS_PER_STORAGE_TYPE_INSTANCE - bit_shift);

            m_words[destination] = value;
        }
    }

    constexpr void shift_right(usize count)
    {
        if (count == 0)
            return;

        if (count >= N) {
            reset();
            return;
        }

        const usize word_shift = count / BITS_PER_STORAGE_TYPE_INSTANCE;
        const usize bit_shift = count % BITS_PER_STORAGE_TYPE_INSTANCE;

        for (usize destination = 0; destination < STORAGE_TYPE_INSTANCE_COUNT; ++destination) {
            const usize source = destination + word_shift;

            if (source >= STORAGE_TYPE_INSTANCE_COUNT) {
                m_words[destination] = 0;
                continue;
            }

            StorageType value = m_words[source] >> bit_shift;
            if (bit_shift != 0 && source + 1 < STORAGE_TYPE_INSTANCE_COUNT)
                value |= m_words[source + 1] << (BITS_PER_STORAGE_TYPE_INSTANCE - bit_shift);

            m_words[destination] = value;
        }
    }

    array<StorageType, STORAGE_TYPE_INSTANCE_COUNT> m_words{};

public:
    constexpr bitmap() = default;

    constexpr __attribute__((always_inline)) void reset()
    {
        for (usize word = 0; word < STORAGE_TYPE_INSTANCE_COUNT; ++word)
            m_words[word] = StorageType{0};
    }

    constexpr __attribute__((always_inline)) void fill()
    {
        for (usize word = 0; word < STORAGE_TYPE_INSTANCE_COUNT; ++word)
            m_words[word] = std::numeric_limits<StorageType>::max();
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) bool empty() const
    {
        for (usize word = 0; word < STORAGE_TYPE_INSTANCE_COUNT; ++word) {
            if (m_words[word] != 0)
                return false;
        }

        return true;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) bool none() const
    {
        return empty();
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) bool any() const
    {
        return !empty();
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) bool all() const
    {
        for (usize word = 0; word < STORAGE_TYPE_INSTANCE_COUNT; ++word) {
            if (m_words[word] != std::numeric_limits<StorageType>::max())
                return false;
        }

        return true;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) bool test(usize index) const
    {
        return (m_words[word_of(index)] & bit_of(index)) != 0;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) bool operator[](usize index) const
    {
        return test(index);
    }

    constexpr void __attribute__((always_inline)) set(usize index)
    {
        m_words[word_of(index)] |= bit_of(index);
    }

    constexpr void __attribute__((always_inline)) clear(usize index)
    {
        m_words[word_of(index)] &= ~bit_of(index);
    }

    constexpr void __attribute__((always_inline)) toggle(usize index)
    {
        m_words[word_of(index)] ^= bit_of(index);
    }

    constexpr void __attribute__((always_inline)) assign(usize index, bool value)
    {
        value ? set(index) : clear(index);
    }

    constexpr void __attribute__((always_inline)) set(usize first, usize count)
    {
        apply(first, count, true);
    }

    constexpr void __attribute__((always_inline)) clear(usize first, usize count)
    {
        apply(first, count, false);
    }

    [[nodiscard]] constexpr usize __attribute__((always_inline)) find_set(usize first_word,
                                                                          usize words) const
    {
        for (usize i = 0; i < words; ++i) {
            const usize       word = first_word + i;
            const StorageType value = m_words[word];

            if (value != 0)
                return word * BITS_PER_STORAGE_TYPE_INSTANCE + countr_zero(value);
        }

        return NPOS;
    }

    [[nodiscard]] constexpr usize find_set_in(usize first, usize count) const
    {
        if (count == 0)
            return NPOS;

        const usize last = first + count - 1;
        const usize first_word = word_of(first);
        const usize last_word = word_of(last);

        if (first_word == last_word) {
            const StorageType selected = m_words[first_word] & mask_from(first) & mask_upto(last);

            return selected != 0
                       ? first_word * BITS_PER_STORAGE_TYPE_INSTANCE + countr_zero(selected)
                       : NPOS;
        }

        {
            const StorageType selected = m_words[first_word] & mask_from(first);

            if (selected != 0)
                return first_word * BITS_PER_STORAGE_TYPE_INSTANCE + countr_zero(selected);
        }

        for (usize word = first_word + 1; word < last_word; ++word) {
            const StorageType value = m_words[word];

            if (value != 0)
                return word * BITS_PER_STORAGE_TYPE_INSTANCE + countr_zero(value);
        }

        {
            const StorageType selected = m_words[last_word] & mask_upto(last);

            if (selected != 0)
                return last_word * BITS_PER_STORAGE_TYPE_INSTANCE + countr_zero(selected);
        }

        return NPOS;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) usize count_set() const
    {
        return count_set(0, STORAGE_TYPE_INSTANCE_COUNT);
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) usize count_set(usize first_word,
                                                                           usize words) const
    {
        usize total = 0;

        for (usize i = 0; i < words; ++i)
            total += std::popcount(m_words[first_word + i]);

        return total;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) usize count_set_in(usize first,
                                                                              usize count) const
    {
        if (count == 0)
            return 0;

        const usize last = first + count - 1;
        const usize first_word = word_of(first);
        const usize last_word = word_of(last);

        if (first_word == last_word) {
            return std::popcount(m_words[first_word] & mask_from(first) & mask_upto(last));
        }

        usize total = 0;

        total += std::popcount(m_words[first_word] & mask_from(first));

        for (usize word = first_word + 1; word < last_word; ++word)
            total += std::popcount(m_words[word]);

        total += std::popcount(m_words[last_word] & mask_upto(last));

        return total;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) StorageType &word(usize index)
    {
        return m_words[index];
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) StorageType word(usize index) const
    {
        return m_words[index];
    }

    // clang-format off
#define DOOM_BITMAP_DEFINE_BITWISE_OPERATOR(op)                                      \
    constexpr __attribute__((always_inline)) bitmap &operator op## = (const bitmap &other)                          \
    {                                                                                \
        for (usize word = 0; word < STORAGE_TYPE_INSTANCE_COUNT; ++word)             \
            m_words[word] op## = other.m_words[word];                                \
                                                                                     \
        return *this;                                                                \
    }                                                                                \
                                                                                     \
    [[nodiscard]] friend constexpr bitmap operator op(bitmap lhs, const bitmap &rhs) \
    {                                                                                \
        lhs op## = rhs;                                                              \
        return lhs;                                                                  \
    }

    DOOM_BITMAP_DEFINE_BITWISE_OPERATOR(|)
    DOOM_BITMAP_DEFINE_BITWISE_OPERATOR(&)
    DOOM_BITMAP_DEFINE_BITWISE_OPERATOR(^)

#undef DOOM_BITMAP_DEFINE_BITWISE_OPERATOR
    // clang-format on

    [[nodiscard]] constexpr __attribute__((always_inline)) bitmap operator~() const
    {
        bitmap result;

        for (usize word = 0; word < STORAGE_TYPE_INSTANCE_COUNT; ++word)
            result.m_words[word] = ~m_words[word];

        return result;
    }

    // clang-format off
#define DOOM_BITMAP_DEFINE_SHIFT_OPERATOR(op, function)                        \
    constexpr __attribute__((always_inline)) bitmap &operator op## = (usize count)                            \
    {                                                                          \
        function(count);                                                       \
        return *this;                                                          \
    }                                                                          \
                                                                               \
    [[nodiscard]] friend constexpr bitmap operator op(bitmap lhs, usize count) \
    {                                                                          \
        lhs op## = count;                                                      \
        return lhs;                                                            \
    }

    DOOM_BITMAP_DEFINE_SHIFT_OPERATOR(<<, shift_left)
    DOOM_BITMAP_DEFINE_SHIFT_OPERATOR(>>, shift_right)

#undef DOOM_BITMAP_DEFINE_SHIFT_OPERATOR
    // clang-format on

    [[nodiscard]] constexpr __attribute__((always_inline)) bool operator==(
        const bitmap &other) const
    {
        for (usize word = 0; word < STORAGE_TYPE_INSTANCE_COUNT; ++word) {
            if (m_words[word] != other.m_words[word])
                return false;
        }

        return true;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) bool operator!=(
        const bitmap &other) const
    {
        return !(*this == other);
    }
};

}  // namespace kernel::core::utils

#endif  // DOOM_OS_KERNEL_CORE_BITMAP_HPP_
