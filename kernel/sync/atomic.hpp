#ifndef DOOM_OS_KERNEL_SYNC_ATOMIC_HPP_
#define DOOM_OS_KERNEL_SYNC_ATOMIC_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/atomic.hpp"
#include "kernel/core/types.hpp"
#include "kernel/utils/traits.hpp"

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
    RELAXED,
    CONSUME,
    ACQUIRE,
    RELEASE,
    ACQ_REL,
    SEQ_CST,
};

// =================================================================================================
// Impl detail
// =================================================================================================

namespace detail {

constexpr kernel::arch::x86_64::atomic::memory_order to_arch_order(memory_order order) {
    using arch_order = kernel::arch::x86_64::atomic::memory_order;

    switch (order) {
        case memory_order::RELAXED:
            return arch_order::RELAXED;
        case memory_order::CONSUME:
            return arch_order::CONSUME;
        case memory_order::ACQUIRE:
            return arch_order::ACQUIRE;
        case memory_order::RELEASE:
            return arch_order::RELEASE;
        case memory_order::ACQ_REL:
            return arch_order::ACQ_REL;
        case memory_order::SEQ_CST:
            return arch_order::SEQ_CST;
    }

    return arch_order::SEQ_CST;
}

constexpr kernel::arch::x86_64::atomic::memory_order to_arch_load_order(memory_order order) {
    using arch_order = kernel::arch::x86_64::atomic::memory_order;

    switch (order) {
        case memory_order::RELAXED:
            return arch_order::RELAXED;
        case memory_order::CONSUME:
            return arch_order::CONSUME;
        case memory_order::ACQUIRE:
            return arch_order::ACQUIRE;
        case memory_order::SEQ_CST:
            return arch_order::SEQ_CST;

        case memory_order::RELEASE:
        case memory_order::ACQ_REL:
            return arch_order::SEQ_CST;
    }

    return arch_order::SEQ_CST;
}

constexpr kernel::arch::x86_64::atomic::memory_order to_arch_store_order(memory_order order) {
    using arch_order = kernel::arch::x86_64::atomic::memory_order;

    switch (order) {
        case memory_order::RELAXED:
            return arch_order::RELAXED;
        case memory_order::RELEASE:
            return arch_order::RELEASE;
        case memory_order::SEQ_CST:
            return arch_order::SEQ_CST;

        case memory_order::CONSUME:
        case memory_order::ACQUIRE:
        case memory_order::ACQ_REL:
            return arch_order::SEQ_CST;
    }

    return arch_order::SEQ_CST;
}

constexpr memory_order default_failure_order(memory_order success_order) {
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

constexpr kernel::arch::x86_64::atomic::memory_order to_arch_failure_order(memory_order order) {
    using arch_order = kernel::arch::x86_64::atomic::memory_order;

    switch (order) {
        case memory_order::RELAXED:
            return arch_order::RELAXED;
        case memory_order::CONSUME:
            return arch_order::CONSUME;
        case memory_order::ACQUIRE:
            return arch_order::ACQUIRE;
        case memory_order::SEQ_CST:
            return arch_order::SEQ_CST;
        case memory_order::RELEASE:
            return arch_order::RELAXED;
        case memory_order::ACQ_REL:
            return arch_order::ACQUIRE;
    }

    return arch_order::SEQ_CST;
}

// =================================================================================================
// Inline atomic storage
// =================================================================================================
//
// The public atomic<T> supports object sizes from 1 to 8 bytes for now.
// Non-power-of-two object sizes are stored in the next larger native atomic storage word.
//
// Examples:
//   sizeof(T) == 1 -> u8
//   sizeof(T) == 2 -> u16
//   sizeof(T) == 3 -> u32
//   sizeof(T) == 4 -> u32
//   sizeof(T) == 5 -> u64
//   sizeof(T) == 6 -> u64
//   sizeof(T) == 7 -> u64
//   sizeof(T) == 8 -> u64
//
// Larger objects should later use a lock-backed fallback.

template <u64 Size>
struct inline_atomic_storage {
    static_assert(Size >= 1 && Size <= 8,
                  "inline atomic storage supports only byte sequences of 1..8 bytes");

    using type = kernel::core::conditional_t<
        (Size <= 1), u8,
        kernel::core::conditional_t<(Size <= 2), u16,
                                    kernel::core::conditional_t<(Size <= 4), u32, u64>>>;
};

template <typename T>
using inline_atomic_storage_t = typename inline_atomic_storage<sizeof(T)>::type;

template <typename T>
struct has_inline_atomic_storage
    : kernel::core::integral_constant<bool, sizeof(T) >= 1 && sizeof(T) <= 8> {};

template <typename T>
inline constexpr bool has_inline_atomic_storage_v = has_inline_atomic_storage<T>::value;

// =================================================================================================
// Atomic object support
// =================================================================================================

template <typename T>
struct supports_atomic_object_ops
    : kernel::core::integral_constant<
          bool, !kernel::core::is_lvalue_reference_v<T> &&
                    !kernel::core::is_rvalue_reference_v<T> &&
                    !kernel::core::is_const_v<kernel::core::remove_reference_t<T>> &&
                    !kernel::core::is_volatile_v<kernel::core::remove_reference_t<T>> &&
                    __is_trivially_copyable(kernel::core::remove_cvref_t<T>) &&
                    has_inline_atomic_storage_v<kernel::core::remove_cvref_t<T>>> {};

template <typename T>
inline constexpr bool supports_atomic_object_ops_v = supports_atomic_object_ops<T>::value;

// =================================================================================================
// Atomic arithmetic support
// =================================================================================================

template <typename T>
struct supports_arithmetic_ops : kernel::core::is_integer<T> {};

template <typename T>
inline constexpr bool supports_arithmetic_ops_v = supports_arithmetic_ops<T>::value;

// =================================================================================================
// Padding canonicalization
// =================================================================================================

template <typename T>
constexpr T canonicalize_atomic_value(T value) {
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

template <typename T>
struct supports_direct_storage_cast
    : kernel::core::integral_constant<
          bool, kernel::core::is_integer_v<T> ||
                    kernel::core::is_same_v<kernel::core::remove_cvref_t<T>, bool>> {};

template <typename T>
inline constexpr bool supports_direct_storage_cast_v = supports_direct_storage_cast<T>::value;

template <typename T>
union atomic_object_storage {
    u8 bytes[sizeof(T)];
    T  value;

    constexpr atomic_object_storage() : bytes{} {}
};

template <typename T>
constexpr inline_atomic_storage_t<T> encode_atomic_value(T value) {
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
constexpr T decode_atomic_value(inline_atomic_storage_t<T> storage) {
    using storage_type = inline_atomic_storage_t<T>;

    if constexpr (supports_direct_storage_cast_v<T>) {
        return static_cast<T>(storage);
    } else {
        atomic_object_storage<T> object{};
        __builtin_memcpy(&object.value, &storage, sizeof(T));

        return canonicalize_atomic_value(object.value);
    }
}

}  // namespace detail

// =================================================================================================
// Barriers
// =================================================================================================

inline void compiler_barrier() { kernel::arch::x86_64::atomic::compiler_barrier(); }

inline void cpu_relax() { kernel::arch::x86_64::atomic::cpu_relax(); }

inline void thread_fence(memory_order order = memory_order::SEQ_CST) {
    kernel::arch::x86_64::atomic::thread_fence(detail::to_arch_order(order));
}

inline void signal_fence(memory_order order = memory_order::SEQ_CST) {
    kernel::arch::x86_64::atomic::signal_fence(detail::to_arch_order(order));
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

    template <typename U = T,
              kernel::core::enable_if_t<kernel::core::is_default_constructible_v<U>, i32> = 0>
    constexpr atomic() : value_m(detail::encode_atomic_value(T{})) {}

    constexpr atomic(T value) : value_m(detail::encode_atomic_value(value)) {}

    atomic(const atomic &) = delete;
    atomic &operator=(const atomic &) = delete;

    atomic(atomic &&) = delete;
    atomic &operator=(atomic &&) = delete;

    T operator=(T value) {
        store(value);
        return value;
    }

    explicit operator T() const { return load(); }

    T load(memory_order order = memory_order::SEQ_CST) const {
        const storage_type storage =
            kernel::arch::x86_64::atomic::load(&value_m, detail::to_arch_load_order(order));

        return detail::decode_atomic_value<T>(storage);
    }

    void store(T value, memory_order order = memory_order::SEQ_CST) {
        const storage_type storage = detail::encode_atomic_value(value);

        kernel::arch::x86_64::atomic::store(&value_m, storage, detail::to_arch_store_order(order));
    }

    T exchange(T value, memory_order order = memory_order::SEQ_CST) {
        const storage_type desired = detail::encode_atomic_value(value);
        const storage_type old =
            kernel::arch::x86_64::atomic::exchange(&value_m, desired, detail::to_arch_order(order));

        return detail::decode_atomic_value<T>(old);
    }

    bool compare_exchange(T &expected, T desired,
                          memory_order success_order = memory_order::SEQ_CST) {
        return compare_exchange(expected, desired, success_order,
                                detail::default_failure_order(success_order));
    }

    bool compare_exchange(T &expected, T desired, memory_order success_order,
                          memory_order failure_order) {
        storage_type       expected_storage = detail::encode_atomic_value(expected);
        const storage_type desired_storage = detail::encode_atomic_value(desired);

        const bool exchanged = kernel::arch::x86_64::atomic::compare_exchange_strong(
            &value_m, &expected_storage, desired_storage, detail::to_arch_order(success_order),
            detail::to_arch_failure_order(failure_order));

        if (!exchanged)
            expected = detail::decode_atomic_value<T>(expected_storage);

        return exchanged;
    }

    bool compare_exchange_weak(T &expected, T desired,
                               memory_order success_order = memory_order::SEQ_CST) {
        return compare_exchange_weak(expected, desired, success_order,
                                     detail::default_failure_order(success_order));
    }

    bool compare_exchange_weak(T &expected, T desired, memory_order success_order,
                               memory_order failure_order) {
        storage_type       expected_storage = detail::encode_atomic_value(expected);
        const storage_type desired_storage = detail::encode_atomic_value(desired);

        const bool exchanged = kernel::arch::x86_64::atomic::compare_exchange_weak(
            &value_m, &expected_storage, desired_storage, detail::to_arch_order(success_order),
            detail::to_arch_failure_order(failure_order));

        if (!exchanged)
            expected = detail::decode_atomic_value<T>(expected_storage);

        return exchanged;
    }

    template <typename U = T,
              kernel::core::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T fetch_add(T value, memory_order order = memory_order::SEQ_CST) {
        const storage_type encoded_value = detail::encode_atomic_value(value);
        const storage_type old = kernel::arch::x86_64::atomic::fetch_add(
            &value_m, encoded_value, detail::to_arch_order(order));

        return detail::decode_atomic_value<T>(old);
    }

    template <typename U = T,
              kernel::core::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T fetch_sub(T value, memory_order order = memory_order::SEQ_CST) {
        const storage_type encoded_value = detail::encode_atomic_value(value);
        const storage_type old = kernel::arch::x86_64::atomic::fetch_sub(
            &value_m, encoded_value, detail::to_arch_order(order));

        return detail::decode_atomic_value<T>(old);
    }

    template <typename U = T,
              kernel::core::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T fetch_and(T value, memory_order order = memory_order::SEQ_CST) {
        const storage_type encoded_value = detail::encode_atomic_value(value);
        const storage_type old = kernel::arch::x86_64::atomic::fetch_and(
            &value_m, encoded_value, detail::to_arch_order(order));

        return detail::decode_atomic_value<T>(old);
    }

    template <typename U = T,
              kernel::core::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T fetch_or(T value, memory_order order = memory_order::SEQ_CST) {
        const storage_type encoded_value = detail::encode_atomic_value(value);
        const storage_type old = kernel::arch::x86_64::atomic::fetch_or(
            &value_m, encoded_value, detail::to_arch_order(order));

        return detail::decode_atomic_value<T>(old);
    }

    template <typename U = T,
              kernel::core::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T fetch_xor(T value, memory_order order = memory_order::SEQ_CST) {
        const storage_type encoded_value = detail::encode_atomic_value(value);
        const storage_type old = kernel::arch::x86_64::atomic::fetch_xor(
            &value_m, encoded_value, detail::to_arch_order(order));

        return detail::decode_atomic_value<T>(old);
    }

    template <typename U = T,
              kernel::core::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T operator++() {
        return fetch_add(static_cast<T>(1)) + static_cast<T>(1);
    }

    template <typename U = T,
              kernel::core::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T operator++(i32) {
        return fetch_add(static_cast<T>(1));
    }

    template <typename U = T,
              kernel::core::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T operator--() {
        return fetch_sub(static_cast<T>(1)) - static_cast<T>(1);
    }

    template <typename U = T,
              kernel::core::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T operator--(i32) {
        return fetch_sub(static_cast<T>(1));
    }

    template <typename U = T,
              kernel::core::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T operator+=(T value) {
        return fetch_add(value) + value;
    }

    template <typename U = T,
              kernel::core::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T operator-=(T value) {
        return fetch_sub(value) - value;
    }

    template <typename U = T,
              kernel::core::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T operator&=(T value) {
        return fetch_and(value) & value;
    }

    template <typename U = T,
              kernel::core::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T operator|=(T value) {
        return fetch_or(value) | value;
    }

    template <typename U = T,
              kernel::core::enable_if_t<detail::supports_arithmetic_ops_v<U>, i32> = 0>
    T operator^=(T value) {
        return fetch_xor(value) ^ value;
    }
};

}  // namespace kernel::sync

#endif  // DOOM_OS_KERNEL_SYNC_ATOMIC_HPP_