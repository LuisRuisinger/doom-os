#ifndef DOOM_OS_KERNEL_CORE_STRING_HPP_
#define DOOM_OS_KERNEL_CORE_STRING_HPP_

#include <limits>

#include "kernel/core/types.hpp"

namespace kernel::core::utils {

template <usize N>
struct fixed_string {
    static_assert(N > 0, "a fixed_string cannot be empty");

    using value_type = char;
    using size_type = usize;
    using reference = char &;
    using const_reference = const char &;
    using pointer = char *;
    using const_pointer = const char *;
    using iterator = char *;
    using const_iterator = const char *;

    static constexpr usize CAPACITY = N;
    static constexpr usize NPOS = std::numeric_limits<usize>::max();

    char  m_data[N + 1];
    usize m_size;

    constexpr fixed_string()
        : m_data{},
          m_size(0)
    {
    }

    constexpr fixed_string(const char *text)
        : fixed_string()
    {
        assign(text);
    }

    constexpr fixed_string(const char *text, usize count)
        : fixed_string()
    {
        assign(text, count);
    }

    template <usize M>
    constexpr fixed_string(const fixed_string<M> &other)
        : fixed_string()
    {
        assign(other);
    }

    [[nodiscard]] static constexpr __attribute__((always_inline)) usize capacity()
    {
        return N;
    }

    [[nodiscard]] static constexpr __attribute__((always_inline)) usize max_size()
    {
        return N;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) usize size() const
    {
        return m_size;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) usize length() const
    {
        return m_size;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) bool empty() const
    {
        return m_size == 0;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) bool full() const
    {
        return m_size == N;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) char *data()
    {
        return m_data;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) const char *data() const
    {
        return m_data;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) const char *c_str() const
    {
        return m_data;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) char &operator[](usize index)
    {
        return m_data[index];
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) const char &operator[](usize index) const
    {
        return m_data[index];
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) char &front()
    {
        return m_data[0];
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) const char &front() const
    {
        return m_data[0];
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) char &back()
    {
        return m_data[m_size - 1];
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) const char &back() const
    {
        return m_data[m_size - 1];
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) char *begin()
    {
        return m_data;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) const char *begin() const
    {
        return m_data;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) char *end()
    {
        return m_data + m_size;
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) const char *end() const
    {
        return m_data + m_size;
    }

    constexpr __attribute__((always_inline)) void clear()
    {
        m_size = 0;
        m_data[0] = '\0';
    }

    constexpr fixed_string &assign(const char *text, usize count)
    {
        m_size = 0;

        return append(text, count);
    }

    constexpr __attribute__((always_inline)) fixed_string &assign(const char *text)
    {
        return assign(text, text == nullptr ? 0 : NPOS);
    }

    template <usize M>
    constexpr __attribute__((always_inline)) fixed_string &assign(const fixed_string<M> &other)
    {
        return assign(other.data(), other.size());
    }

    constexpr fixed_string &append(const char *text, usize count)
    {
        for (usize i = 0; i < count && m_size < N && text[i] != '\0'; ++i)
            m_data[m_size++] = text[i];

        m_data[m_size] = '\0';

        return *this;
    }

    constexpr __attribute__((always_inline)) fixed_string &append(const char *text)
    {
        return append(text, text == nullptr ? 0 : NPOS);
    }

    template <usize M>
    constexpr __attribute__((always_inline)) fixed_string &append(const fixed_string<M> &other)
    {
        return append(other.data(), other.size());
    }

    constexpr __attribute__((always_inline)) fixed_string &append(char c)
    {
        return append(&c, 1);
    }

    constexpr __attribute__((always_inline)) void push_back(char c)
    {
        append(c);
    }

    constexpr __attribute__((always_inline)) void pop_back()
    {
        m_data[--m_size] = '\0';
    }

    constexpr __attribute__((always_inline)) fixed_string &operator+=(const char *text)
    {
        return append(text);
    }

    constexpr __attribute__((always_inline)) fixed_string &operator+=(char c)
    {
        return append(c);
    }

    template <usize M>
    constexpr __attribute__((always_inline)) fixed_string &operator+=(const fixed_string<M> &other)
    {
        return append(other);
    }

    [[nodiscard]] constexpr int compare(const char *text) const
    {
        usize i = 0;

        for (; i < m_size && text[i] != '\0'; ++i)
            if (m_data[i] != text[i])
                return m_data[i] < text[i] ? -1 : 1;

        if (i < m_size)
            return 1;

        return text[i] == '\0' ? 0 : -1;
    }

    template <usize M>
    [[nodiscard]] constexpr __attribute__((always_inline)) int compare(
        const fixed_string<M> &other) const
    {
        return compare(other.c_str());
    }

    [[nodiscard]] constexpr __attribute__((always_inline)) bool operator==(const char *text) const
    {
        return compare(text) == 0;
    }

    template <usize M>
    [[nodiscard]] constexpr __attribute__((always_inline)) bool operator==(
        const fixed_string<M> &other) const
    {
        return m_size == other.size() && compare(other) == 0;
    }

    [[nodiscard]] constexpr bool starts_with(const char *prefix) const
    {
        for (usize i = 0; prefix[i] != '\0'; ++i)
            if (i >= m_size || m_data[i] != prefix[i])
                return false;

        return true;
    }

    [[nodiscard]] constexpr bool ends_with(const char *suffix) const
    {
        usize count = 0;

        while (suffix[count] != '\0')
            ++count;

        if (count > m_size)
            return false;

        for (usize i = 0; i < count; ++i)
            if (m_data[m_size - count + i] != suffix[i])
                return false;

        return true;
    }

    [[nodiscard]] constexpr usize find(char c, usize from = 0) const
    {
        for (usize i = from; i < m_size; ++i)
            if (m_data[i] == c)
                return i;

        return NPOS;
    }
};

template <usize M>
fixed_string(const char (&)[M]) -> fixed_string<M - 1>;

}  // namespace kernel::core::utils

#endif  // DOOM_OS_KERNEL_CORE_STRING_HPP_
