#ifndef DOOM_OS_KERNEL_SYNC_ATOMIC_HPP_
#define DOOM_OS_KERNEL_SYNC_ATOMIC_HPP_

// =================================================================================================
// Cpp stdlib files
// =================================================================================================

#include <type_traits>

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"

namespace kernel::sync {

using kernel::core::i32;
using kernel::core::u16;
using kernel::core::u32;
using kernel::core::u64;
using kernel::core::u8;

// =================================================================================================
// Memory order
// =================================================================================================

enum class memory_order : u8 {
    RELAXED = __ATOMIC_RELAXED,
    CONSUME = __ATOMIC_CONSUME,
    ACQUIRE = __ATOMIC_ACQUIRE,
    RELEASE = __ATOMIC_RELEASE,
    ACQ_REL = __ATOMIC_ACQ_REL,
    SEQ_CST = __ATOMIC_SEQ_CST,
};

// =================================================================================================
// Impl detail
// =================================================================================================

namespace detail {

constexpr int to_builtin_order(memory_order order)
{
    return static_cast<int>(order);
}

constexpr int to_load_order(memory_order order)
{
    switch (order) {
        case memory_order::RELAXED:
        case memory_order::CONSUME:
        case memory_order::ACQUIRE:
        case memory_order::SEQ_CST:
            return to_builtin_order(order);

        case memory_order::RELEASE:
        case memory_order::ACQ_REL:
            return __ATOMIC_SEQ_CST;
    }

    return __ATOMIC_SEQ_CST;
}

constexpr int to_store_order(memory_order order)
{
    switch (order) {
        case memory_order::RELAXED:
        case memory_order::RELEASE:
        case memory_order::SEQ_CST:
            return to_builtin_order(order);

        case memory_order::CONSUME:
        case memory_order::ACQUIRE:
        case memory_order::ACQ_REL:
            return __ATOMIC_SEQ_CST;
    }

    return __ATOMIC_SEQ_CST;
}

constexpr memory_order default_failure_order(memory_order success_order)
{
    switch (success_order) {
        case memory_order::RELAXED:
            return memory_order::RELAXED;
        case memory_order::CONSUME:
            return memory_order::CONSUME;
        case memory_order::ACQUIRE:
            return memory_order::ACQUIRE;
        case memory_order::RELEASE:
            return memory_order::RELAXED;
        case memory_order::ACQ_REL:
            return memory_order::ACQUIRE;
        case memory_order::SEQ_CST:
            return memory_order::SEQ_CST;
    }

    return memory_order::SEQ_CST;
}

constexpr int to_failure_order(memory_order order)
{
    switch (order) {
        case memory_order::RELAXED:
        case memory_order::CONSUME:
        case memory_order::ACQUIRE:
        case memory_order::SEQ_CST:
            return to_builtin_order(order);

        case memory_order::RELEASE:
            return __ATOMIC_RELAXED;
        case memory_order::ACQ_REL:
            return __ATOMIC_ACQUIRE;
    }

    return __ATOMIC_SEQ_CST;
}

// =================================================================================================
// Inline atomic storage
//
// The public atomic<T> supports object sizes from 1 to 8 bytes for now.
// Non-power-of-two object sizes are stored in the next larger native atomic storage word.
// =================================================================================================

template <u64 Size>
struct inline_atomic_storage {
    static_assert(Size >= 1 && Size <= 8,
                  "inline atomic storage supports only byte sequences of 1..8 bytes");

    using type = std::conditional_t<
        (Size <= 1), u8,
        std::conditional_t<(Size <= 2), u16, std::conditional_t<(Size <= 4), u32, u64>>>;
};

template <typename T>
using inline_atomic_storage_t = typename inline_atomic_storage<sizeof(T)>::type;

template <typename T>
struct has_inline_atomic_storage : std::integral_constant<bool, sizeof(T) >= 1 && sizeof(T) <= 8> {
};

template <typename T>
inline constexpr bool has_inline_atomic_storage_v = has_inline_atomic_storage<T>::value;

// =================================================================================================
// Atomic object support
// =================================================================================================

template <typename T>
struct supports_atomic_object_ops
    : std::integral_constant<bool, !std::is_lvalue_reference_v<T> &&
                                       !std::is_rvalue_reference_v<T> &&
                                       !std::is_const_v<std::remove_reference_t<T>> &&
                                       !std::is_volatile_v<std::remove_reference_t<T>> &&
                                       __is_trivially_copyable(std::remove_cvref_t<T>) &&
                                       has_inline_atomic_storage_v<std::remove_cvref_t<T>>> {};

template <typename T>
inline constexpr bool supports_atomic_object_ops_v = supports_atomic_object_ops<T>::value;

// =================================================================================================
// Atomic arithmetic support
// =================================================================================================

template <typename T>
struct supports_arithmetic_ops
    : std::integral_constant<bool, std::is_integral_v<T> &&
                                       !std::is_same_v<std::remove_cvref_t<T>, bool>> {};

template <typename T>
inline constexpr bool supports_arithmetic_ops_v = supports_arithmetic_ops<T>::value;

// =================================================================================================
// Padding canonicalization
// =================================================================================================

template <typename T>
constexpr T canonicalize_atomic_value(T value)
{
#if defined(__has_builtin)
#    if __has_builtin(__builtin_clear_padding)
    if (!__builtin_is_constant_evaluated()) {
        __builtin_clear_padding(&value);
    }
#    endif
#elif defined(__GNUC__)
    if (!__builtin_is_constant_evaluated()) {
        __builtin_clear_padding(&value);
    }
#endif

    return value;
}

// =================================================================================================
// Atomic value encoding
// =================================================================================================

// std::is_integral_v already covers bool.
template <typename T>
struct supports_direct_storage_cast : std::bool_constant<std::is_integral_v<T>> {};

template <typename T>
inline constexpr bool supports_direct_storage_cast_v = supports_direct_storage_cast<T>::value;

template <typename T>
union atomic_object_storage {
    u8 bytes[sizeof(T)];
    T  value;

    constexpr atomic_object_storage()
        : bytes{}
    {
    }
};

template <typename T>
constexpr inline_atomic_storage_t<T> encode_atomic_value(T value)
{
    using storage_type = inline_atomic_storage_t<T>;

    if constexpr (supports_direct_storage_cast_v<T>) {
        return static_cast<storage_type>(value);
    } else {
        value = canonicalize_atomic_value(value);

        storage_type storage{};
        __builtin_memcpy(&storage, &value, sizeof(T));

        return storage;
    }
}

template <typename T>
constexpr T decode_atomic_value(inline_atomic_storage_t<T> storage)
{
    if constexpr (supports_direct_storage_cast_v<T>) {
        return static_cast<T>(storage);
    } else {
        atomic_object_storage<T> object{};
        __builtin_memcpy(&object.value, &storage, sizeof(T));

        return canonicalize_atomic_value(object.value);
    }
}

template <typename T>
constexpr T add_atomic_values(T lhs, T rhs)
{
    using storage_type = inline_atomic_storage_t<T>;

    const storage_type lhs_storage = encode_atomic_value(lhs);
    const storage_type rhs_storage = encode_atomic_value(rhs);

    return decode_atomic_value<T>(static_cast<storage_type>(lhs_storage + rhs_storage));
}

template <typename T>
constexpr T sub_atomic_values(T lhs, T rhs)
{
    using storage_type = inline_atomic_storage_t<T>;

    const storage_type lhs_storage = encode_atomic_value(lhs);
    const storage_type rhs_storage = encode_atomic_value(rhs);

    return decode_atomic_value<T>(static_cast<storage_type>(lhs_storage - rhs_storage));
}

}  // namespace detail

// =================================================================================================
// Barriers
// =================================================================================================

inline void compiler_barrier()
{
    asm volatile("" ::: "memory");
}

inline void thread_fence(memory_order order = memory_order::SEQ_CST)
{
    __atomic_thread_fence(detail::to_builtin_order(order));
}

inline void signal_fence(memory_order order = memory_order::SEQ_CST)
{
    __atomic_signal_fence(detail::to_builtin_order(order));
}

// =================================================================================================
// Atomic object
// =================================================================================================

template <typename T>
class atomic {
    static_assert(detail::supports_atomic_object_ops_v<T>,
                  "kernel::sync::atomic<T> currently requires non-cv, non-reference, trivially "
                  "copyable T with size <= 8 bytes");

    using storage_type = detail::inline_atomic_storage_t<T>;
    static_assert(__atomic_always_lock_free(sizeof(storage_type), nullptr),
                  "kernel::sync::atomic<T> requires always-lock-free inline storage");

    alignas(storage_type) storage_type value_m;

public:
    using value_type = T;

    template <typename U = T, std::enable_if_t<std::is_default_constructible_v<U>, i32> = 0>
    constexpr atomic()
        : value_m(detail::encode_atomic_value(T{}))
    {
    }

    constexpr atomic(T value)
        : value_m(detail::encode_atomic_value(value))
    {
    }

    atomic(const atomic &) = delete;
    atomic &operator=(const atomic &) = delete;

    atomic(atomic &&) = delete;
    atomic &operator=(atomic &&) = delete;

    T operator=(T value)
    {
        store(value);
        return value;
    }

    explicit operator T() const
    {
        return load();
    }

    T load(memory_order order = memory_order::SEQ_CST) const
    {
        const storage_type storage = __atomic_load_n(&value_m, detail::to_load_order(order));

        return detail::decode_atomic_value<T>(storage);
    }

    void store(T value, memory_order order = memory_order::SEQ_CST)
    {
        const storage_type storage = detail::encode_atomic_value(value);

        __atomic_store_n(&value_m, storage, detail::to_store_order(order));
    }

    T exchange(T value, memory_order order = memory_order::SEQ_CST)
    {
        const storage_type desired = detail::encode_atomic_value(value);
        const storage_type old =
            __atomic_exchange_n(&value_m, desired, detail::to_builtin_order(order));

        return detail::decode_atomic_value<T>(old);
    }

    bool compare_exchange(T &expected, T desired,
                          memory_order success_order = memory_order::SEQ_CST)
    {
        return compare_exchange(expected, desired, success_order,
                                detail::default_failure_order(success_order));
    }

    bool compare_exchange(T &expected, T desired, memory_order success_order,
                          memory_order failure_order)
    {
        storage_type       expected_storage = detail::encode_atomic_value(expected);
        const storage_type desired_storage = detail::encode_atomic_value(desired);

        const bool exchanged = __atomic_compare_exchange_n(
            &value_m, &expected_storage, desired_storage, false,
            detail::to_builtin_order(success_order), detail::to_failure_order(failure_order));

        if (!exchanged)
            expected = detail::decode_atomic_value<T>(expected_storage);

        return exchanged;
    }

    bool compare_exchange_weak(T &expected, T desired,
                               memory_order success_order = memory_order::SEQ_CST)
    {
        return compare_exchange_weak(expected, desired, success_order,
                                     detail::default_failure_order(success_order));
    }

    bool compare_exchange_weak(T &expected, T desired, memory_order success_order,
                               memory_order failure_order)
    {
        storage_type       expected_storage = detail::encode_atomic_value(expected);
        const storage_type desired_storage = detail::encode_atomic_value(desired);

        const bool exchanged = __atomic_compare_exchange_n(
            &value_m, &expected_storage, desired_storage, true,
            detail::to_builtin_order(success_order), detail::to_failure_order(failure_order));

        if (!exchanged)
            expected = detail::decode_atomic_value<T>(expected_storage);

        return exchanged;
    }

    template <typename U = T, std::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T fetch_add(T value, memory_order order = memory_order::SEQ_CST)
    {
        const storage_type encoded_value = detail::encode_atomic_value(value);
        const storage_type old =
            __atomic_fetch_add(&value_m, encoded_value, detail::to_builtin_order(order));

        return detail::decode_atomic_value<T>(old);
    }

    template <typename U = T, std::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T fetch_sub(T value, memory_order order = memory_order::SEQ_CST)
    {
        const storage_type encoded_value = detail::encode_atomic_value(value);
        const storage_type old =
            __atomic_fetch_sub(&value_m, encoded_value, detail::to_builtin_order(order));

        return detail::decode_atomic_value<T>(old);
    }

    template <typename U = T, std::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T fetch_and(T value, memory_order order = memory_order::SEQ_CST)
    {
        const storage_type encoded_value = detail::encode_atomic_value(value);
        const storage_type old =
            __atomic_fetch_and(&value_m, encoded_value, detail::to_builtin_order(order));

        return detail::decode_atomic_value<T>(old);
    }

    template <typename U = T, std::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T fetch_or(T value, memory_order order = memory_order::SEQ_CST)
    {
        const storage_type encoded_value = detail::encode_atomic_value(value);
        const storage_type old =
            __atomic_fetch_or(&value_m, encoded_value, detail::to_builtin_order(order));

        return detail::decode_atomic_value<T>(old);
    }

    template <typename U = T, std::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T fetch_xor(T value, memory_order order = memory_order::SEQ_CST)
    {
        const storage_type encoded_value = detail::encode_atomic_value(value);
        const storage_type old =
            __atomic_fetch_xor(&value_m, encoded_value, detail::to_builtin_order(order));

        return detail::decode_atomic_value<T>(old);
    }

    template <typename U = T, std::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T operator++()
    {
        return detail::add_atomic_values(fetch_add(static_cast<T>(1)), static_cast<T>(1));
    }

    template <typename U = T, std::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T operator++(i32)
    {
        return fetch_add(static_cast<T>(1));
    }

    template <typename U = T, std::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T operator--()
    {
        return detail::sub_atomic_values(fetch_sub(static_cast<T>(1)), static_cast<T>(1));
    }

    template <typename U = T, std::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T operator--(i32)
    {
        return fetch_sub(static_cast<T>(1));
    }

    template <typename U = T, std::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T operator+=(T value)
    {
        return detail::add_atomic_values(fetch_add(value), value);
    }

    template <typename U = T, std::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T operator-=(T value)
    {
        return detail::sub_atomic_values(fetch_sub(value), value);
    }

    template <typename U = T, std::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T operator&=(T value)
    {
        return detail::decode_atomic_value<T>(static_cast<storage_type>(
            detail::encode_atomic_value(fetch_and(value)) & detail::encode_atomic_value(value)));
    }

    template <typename U = T, std::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T operator|=(T value)
    {
        return detail::decode_atomic_value<T>(static_cast<storage_type>(
            detail::encode_atomic_value(fetch_or(value)) | detail::encode_atomic_value(value)));
    }

    template <typename U = T, std::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T operator^=(T value)
    {
        return detail::decode_atomic_value<T>(static_cast<storage_type>(
            detail::encode_atomic_value(fetch_xor(value)) ^ detail::encode_atomic_value(value)));
    }
};

}  // namespace kernel::sync

#endif  // DOOM_OS_KERNEL_SYNC_ATOMIC_HPP_