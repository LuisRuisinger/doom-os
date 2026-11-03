#ifndef DOOM_OS_KERNEL_CORE_UTILS_INPLACE_VECTOR_HPP_
#define DOOM_OS_KERNEL_CORE_UTILS_INPLACE_VECTOR_HPP_

// =================================================================================================
// Standard library
// =================================================================================================

#include <memory>
#include <source_location>
#include <utility>

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/result.hpp"
#include "kernel/core/types.hpp"
#include "kernel/core/utils/container.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/mm/page.hpp"

namespace kernel::core::utils {

template <typename T, usize N>
struct inplace_vector {
    static_assert(N > 0, "an inplace_vector cannot have zero capacity");

    using value_type = T;
    using size_type = usize;
    using reference = T &;
    using const_reference = const T &;
    using pointer = T *;
    using const_pointer = const T *;
    using iterator = T *;
    using const_iterator = const T *;

    T     m_items[N]{};
    usize m_size{};

    [[nodiscard, gnu::always_inline]] static constexpr usize capacity()
    {
        return N;
    }

    [[nodiscard, gnu::always_inline]] static constexpr usize max_size()
    {
        return N;
    }

    [[nodiscard, gnu::always_inline]] constexpr usize size() const
    {
        return m_size;
    }

    [[nodiscard, gnu::always_inline]] constexpr bool empty() const
    {
        return m_size == 0;
    }

    [[nodiscard, gnu::always_inline]] constexpr T *data()
    {
        return m_items;
    }

    [[nodiscard, gnu::always_inline]] constexpr const T *data() const
    {
        return m_items;
    }

    [[nodiscard, gnu::always_inline]] constexpr T &operator[](usize index)
    {
        return m_items[index];
    }

    [[nodiscard, gnu::always_inline]] constexpr const T &operator[](usize index) const
    {
        return m_items[index];
    }

    [[nodiscard, gnu::always_inline]] constexpr T &front()
    {
        return m_items[0];
    }

    [[nodiscard, gnu::always_inline]] constexpr const T &front() const
    {
        return m_items[0];
    }

    [[nodiscard, gnu::always_inline]] constexpr T &back()
    {
        return m_items[m_size - 1];
    }

    [[nodiscard, gnu::always_inline]] constexpr const T &back() const
    {
        return m_items[m_size - 1];
    }

    [[nodiscard, gnu::always_inline]] constexpr T *begin()
    {
        return m_items;
    }

    [[nodiscard, gnu::always_inline]] constexpr const T *begin() const
    {
        return m_items;
    }

    [[nodiscard, gnu::always_inline]] constexpr T *end()
    {
        return m_items + m_size;
    }

    [[nodiscard, gnu::always_inline]] constexpr const T *end() const
    {
        return m_items + m_size;
    }

    template <typename... Args>
    constexpr T &emplace_back(Args &&...args)  // TODO currently no std::source_location support
    {
        if (m_size == N)
            KPANIC("inplace_vector: emplace_back past capacity");

        T *slot = m_items + m_size;

        std::destroy_at(slot);
        std::construct_at(slot, std::forward<Args>(args)...);

        ++m_size;
        return *slot;
    }

    constexpr T &push_back(const T &value)
    {
        if (m_size == N)
            KPANIC("inplace_vector: push_back past capacity");

        return emplace_back(value);
    }

    constexpr void pop_back()
    {
        if (m_size == 0)
            KPANIC("inplace_vector: pop_back on empty");

        --m_size;
    }

    Result<T &, container_error> try_push_back(const T &value)
    {
        if (m_size == N)
            return Err(container_error::CAPACITY_EXCEEDED);

        return Ok(emplace_back(value));
    }

    Result<void, container_error> try_pop_back()
    {
        if (m_size == 0)
            return Err(container_error::EMPTY);

        return Ok();
    }

    [[gnu::always_inline]] constexpr void clear()
    {
        m_size = 0;
    }
};

}  // namespace kernel::core::utils

#endif  // DOOM_OS_KERNEL_CORE_UTILS_INPLACE_VECTOR_HPP_
