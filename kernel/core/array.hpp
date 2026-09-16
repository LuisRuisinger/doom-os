#ifndef DOOM_OS_KERNEL_CORE_ARRAY_HPP_
#define DOOM_OS_KERNEL_CORE_ARRAY_HPP_

#include <type_traits>

#include "kernel/core/types.hpp"

namespace kernel::core::utils {

template <typename T, usize N>
struct array {
    static_assert(N > 0, "an array cannot be empty");

    using value_type = T;
    using size_type = usize;
    using reference = T &;
    using const_reference = const T &;
    using pointer = T *;
    using const_pointer = const T *;
    using iterator = T *;
    using const_iterator = const T *;

    T m_items[N];

    [[nodiscard]] static constexpr __attribute__((always_inline)) usize size()
    {
        return N;
    }

    [[nodiscard]] static constexpr __attribute__((always_inline)) usize max_size()
    {
        return N;
    }

    [[nodiscard]] static constexpr __attribute__((always_inline)) bool empty()
    {
        return false;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) T *data()
    {
        return m_items;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) const T *data() const
    {
        return m_items;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) T &operator[](usize index)
    {
        return m_items[index];
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) const T &operator[](usize index) const
    {
        return m_items[index];
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) T &front()
    {
        return m_items[0];
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) const T &front() const
    {
        return m_items[0];
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) T &back()
    {
        return m_items[N - 1];
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) const T &back() const
    {
        return m_items[N - 1];
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) T *begin()
    {
        return m_items;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) const T *begin() const
    {
        return m_items;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) T *end()
    {
        return m_items + N;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) const T *end() const
    {
        return m_items + N;
    }

    constexpr __attribute__((always_inline)) void fill(const T &value)
    {
        for (T &item : m_items)
            item = value;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) bool operator==(const array &other) const
    {
        for (usize i = 0; i < N; ++i)
            if (!(m_items[i] == other.m_items[i]))
                return false;

        return true;
    }
};

template <typename T, typename... Ts>
array(T, Ts...) -> array<T, 1 + sizeof...(Ts)>;

}  // namespace kernel::core::utils

#endif  // DOOM_OS_KERNEL_CORE_ARRAY_HPP_
