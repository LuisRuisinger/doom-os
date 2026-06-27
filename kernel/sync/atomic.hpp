#ifndef DOOM_OS_KERNEL_SYNC_ATOMIC_HPP_
#define DOOM_OS_KERNEL_SYNC_ATOMIC_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/atomic.hpp"
#include "kernel/core/types.hpp"

namespace kernel::sync {

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

template <typename T>
constexpr bool supported_atomic_size() {
    return sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8;
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
   public:
    static_assert(detail::supported_atomic_size<T>(),
                  "kernel::sync::atomic<T> supports only 1, 2, 4, or 8 byte scalar types");

    atomic() = default;

    constexpr explicit atomic(T value) : value_m(value) {}

    atomic(const atomic &) = delete;
    atomic &operator=(const atomic &) = delete;

    atomic(const T &&t) { value_m = t; }

    T load(memory_order order = memory_order::SEQ_CST) const {
        return kernel::arch::x86_64::atomic::load(&value_m, detail::to_arch_load_order(order));
    }

    void store(T value, memory_order order = memory_order::SEQ_CST) {
        kernel::arch::x86_64::atomic::store(&value_m, value, detail::to_arch_store_order(order));
    }

    T exchange(T value, memory_order order = memory_order::SEQ_CST) {
        return kernel::arch::x86_64::atomic::exchange(&value_m, value,
                                                      detail::to_arch_order(order));
    }

    bool compare_exchange(T &expected, T desired,
                          memory_order success_order = memory_order::SEQ_CST) {
        return compare_exchange(expected, desired, success_order,
                                detail::default_failure_order(success_order));
    }

    bool compare_exchange(T &expected, T desired, memory_order success_order,
                          memory_order failure_order) {
        return kernel::arch::x86_64::atomic::compare_exchange_strong(
            &value_m, &expected, desired, detail::to_arch_order(success_order),
            detail::to_arch_failure_order(failure_order));
    }

    bool compare_exchange_weak(T &expected, T desired,
                               memory_order success_order = memory_order::SEQ_CST) {
        return compare_exchange_weak(expected, desired, success_order,
                                     detail::default_failure_order(success_order));
    }

    bool compare_exchange_weak(T &expected, T desired, memory_order success_order,
                               memory_order failure_order) {
        return kernel::arch::x86_64::atomic::compare_exchange_weak(
            &value_m, &expected, desired, detail::to_arch_order(success_order),
            detail::to_arch_failure_order(failure_order));
    }

    T fetch_add(T value, memory_order order = memory_order::SEQ_CST) {
        return kernel::arch::x86_64::atomic::fetch_add(&value_m, value,
                                                       detail::to_arch_order(order));
    }

    T fetch_sub(T value, memory_order order = memory_order::SEQ_CST) {
        return kernel::arch::x86_64::atomic::fetch_sub(&value_m, value,
                                                       detail::to_arch_order(order));
    }

    T fetch_and(T value, memory_order order = memory_order::SEQ_CST) {
        return kernel::arch::x86_64::atomic::fetch_and(&value_m, value,
                                                       detail::to_arch_order(order));
    }

    T fetch_or(T value, memory_order order = memory_order::SEQ_CST) {
        return kernel::arch::x86_64::atomic::fetch_or(&value_m, value,
                                                      detail::to_arch_order(order));
    }

    T fetch_xor(T value, memory_order order = memory_order::SEQ_CST) {
        return kernel::arch::x86_64::atomic::fetch_xor(&value_m, value,
                                                       detail::to_arch_order(order));
    }

   private:
    alignas(T) T value_m{};
};

}  // namespace kernel::sync

#endif  // DOOM_OS_KERNEL_SYNC_ATOMIC_HPP_