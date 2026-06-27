#ifndef DOOM_OS_KERNEL_ARCH_X86_64_ATOMIC_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_ATOMIC_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"

namespace kernel::arch::x86_64::atomic {

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

namespace detail {

constexpr int to_builtin_order(memory_order order) {
    switch (order) {
        case memory_order::RELAXED:
            return __ATOMIC_RELAXED;
        case memory_order::CONSUME:
            return __ATOMIC_CONSUME;
        case memory_order::ACQUIRE:
            return __ATOMIC_ACQUIRE;
        case memory_order::RELEASE:
            return __ATOMIC_RELEASE;
        case memory_order::ACQ_REL:
            return __ATOMIC_ACQ_REL;
        case memory_order::SEQ_CST:
            return __ATOMIC_SEQ_CST;
    }

    return __ATOMIC_SEQ_CST;
}

constexpr int to_load_order(memory_order order) {
    switch (order) {
        case memory_order::RELAXED:
            return __ATOMIC_RELAXED;
        case memory_order::CONSUME:
            return __ATOMIC_CONSUME;
        case memory_order::ACQUIRE:
            return __ATOMIC_ACQUIRE;
        case memory_order::SEQ_CST:
            return __ATOMIC_SEQ_CST;

        case memory_order::RELEASE:
        case memory_order::ACQ_REL:
            return __ATOMIC_SEQ_CST;
    }

    return __ATOMIC_SEQ_CST;
}

constexpr int to_store_order(memory_order order) {
    switch (order) {
        case memory_order::RELAXED:
            return __ATOMIC_RELAXED;
        case memory_order::RELEASE:
            return __ATOMIC_RELEASE;
        case memory_order::SEQ_CST:
            return __ATOMIC_SEQ_CST;

        case memory_order::CONSUME:
        case memory_order::ACQUIRE:
        case memory_order::ACQ_REL:
            return __ATOMIC_SEQ_CST;
    }

    return __ATOMIC_SEQ_CST;
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

template <typename T>
constexpr bool supported_atomic_size() {
    return sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8;
}

}  // namespace detail

// =================================================================================================
// Barriers
// =================================================================================================

inline void compiler_barrier() { asm volatile("" ::: "memory"); }

inline void cpu_relax() { asm volatile("pause" ::: "memory"); }

inline void thread_fence(memory_order order = memory_order::SEQ_CST) {
    __atomic_thread_fence(detail::to_builtin_order(order));
}

inline void signal_fence(memory_order order = memory_order::SEQ_CST) {
    __atomic_signal_fence(detail::to_builtin_order(order));
}

// =================================================================================================
// Basic atomic operations
// =================================================================================================

template <typename T>
inline T load(const T *address, memory_order order = memory_order::SEQ_CST) {
    static_assert(detail::supported_atomic_size<T>(), "unsupported atomic type size");

    return __atomic_load_n(address, detail::to_load_order(order));
}

template <typename T>
inline void store(T *address, T value, memory_order order = memory_order::SEQ_CST) {
    static_assert(detail::supported_atomic_size<T>(), "unsupported atomic type size");

    __atomic_store_n(address, value, detail::to_store_order(order));
}

template <typename T>
inline T exchange(T *address, T value, memory_order order = memory_order::SEQ_CST) {
    static_assert(detail::supported_atomic_size<T>(), "unsupported atomic type size");

    return __atomic_exchange_n(address, value, detail::to_builtin_order(order));
}

template <typename T>
inline bool compare_exchange_strong(T *address, T *expected, T desired,
                                    memory_order success_order = memory_order::SEQ_CST,
                                    memory_order failure_order = memory_order::SEQ_CST) {
    static_assert(detail::supported_atomic_size<T>(), "unsupported atomic type size");

    return __atomic_compare_exchange_n(address, expected, desired, false,
                                       detail::to_builtin_order(success_order),
                                       detail::to_builtin_order(failure_order));
}

template <typename T>
inline bool compare_exchange_strong(T *address, T *expected, T desired,
                                    memory_order success_order) {
    return compare_exchange_strong(address, expected, desired, success_order,
                                   detail::default_failure_order(success_order));
}

template <typename T>
inline bool compare_exchange_weak(T *address, T *expected, T desired,
                                  memory_order success_order = memory_order::SEQ_CST,
                                  memory_order failure_order = memory_order::SEQ_CST) {
    static_assert(detail::supported_atomic_size<T>(), "unsupported atomic type size");

    return __atomic_compare_exchange_n(address, expected, desired, true,
                                       detail::to_builtin_order(success_order),
                                       detail::to_builtin_order(failure_order));
}

template <typename T>
inline bool compare_exchange_weak(T *address, T *expected, T desired, memory_order success_order) {
    return compare_exchange_weak(address, expected, desired, success_order,
                                 detail::default_failure_order(success_order));
}

// =================================================================================================
// Arithmetic atomic operations
// =================================================================================================

template <typename T>
inline T fetch_add(T *address, T value, memory_order order = memory_order::SEQ_CST) {
    static_assert(detail::supported_atomic_size<T>(), "unsupported atomic type size");

    return __atomic_fetch_add(address, value, detail::to_builtin_order(order));
}

template <typename T>
inline T fetch_sub(T *address, T value, memory_order order = memory_order::SEQ_CST) {
    static_assert(detail::supported_atomic_size<T>(), "unsupported atomic type size");

    return __atomic_fetch_sub(address, value, detail::to_builtin_order(order));
}

// =================================================================================================
// Bitwise atomic operations
// =================================================================================================

template <typename T>
inline T fetch_and(T *address, T value, memory_order order = memory_order::SEQ_CST) {
    static_assert(detail::supported_atomic_size<T>(), "unsupported atomic type size");

    return __atomic_fetch_and(address, value, detail::to_builtin_order(order));
}

template <typename T>
inline T fetch_or(T *address, T value, memory_order order = memory_order::SEQ_CST) {
    static_assert(detail::supported_atomic_size<T>(), "unsupported atomic type size");

    return __atomic_fetch_or(address, value, detail::to_builtin_order(order));
}

template <typename T>
inline T fetch_xor(T *address, T value, memory_order order = memory_order::SEQ_CST) {
    static_assert(detail::supported_atomic_size<T>(), "unsupported atomic type size");

    return __atomic_fetch_xor(address, value, detail::to_builtin_order(order));
}

}  // namespace kernel::arch::x86_64::atomic

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_ATOMIC_HPP_