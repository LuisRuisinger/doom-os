#ifndef DOOM_OS_KERNEL_SYNC_SPINLOCK_HPP_
#define DOOM_OS_KERNEL_SYNC_SPINLOCK_HPP_

#include "arch/x86_64/cpu/cpu.hpp"
#include "kernel/core/types.hpp"
#include "kernel/sync/atomic.hpp"

namespace kernel::sync {

using kernel::core::u8;

class spinlock {
    static constexpr u8 SPINLOCK_UNLOCKED = 0;
    static constexpr u8 SPINLOCK_LOCKED = 1;

    atomic<u8> m_state{SPINLOCK_UNLOCKED};

public:
    constexpr spinlock() = default;

    spinlock(const spinlock &) = delete;
    spinlock &operator=(const spinlock &) = delete;

    spinlock(spinlock &&) = delete;
    spinlock &operator=(spinlock &&) = delete;

    void lock()
    {
        for (;;) {
            if (try_lock())
                return;

            while (m_state.load(memory_order::RELAXED) == SPINLOCK_LOCKED)
                kernel::arch::x86_64::cpu::relax();
        }
    }

    bool try_lock()
    {
        u8 expected = SPINLOCK_UNLOCKED;
        return m_state.compare_exchange(expected, SPINLOCK_LOCKED, memory_order::ACQUIRE,
                                        memory_order::RELAXED);
    }

    void unlock()
    {
        m_state.store(SPINLOCK_UNLOCKED, memory_order::RELEASE);
    }

    bool is_locked() const
    {
        return m_state.load(memory_order::RELAXED) == SPINLOCK_LOCKED;
    }
};

class spinlock_guard {
    spinlock &lock_m;

public:
    explicit spinlock_guard(spinlock &lock)
        : lock_m(lock)
    {
        lock_m.lock();
    }

    ~spinlock_guard()
    {
        lock_m.unlock();
    }

    spinlock_guard(const spinlock_guard &) = delete;
    spinlock_guard &operator=(const spinlock_guard &) = delete;

    spinlock_guard(spinlock_guard &&) = delete;
    spinlock_guard &operator=(spinlock_guard &&) = delete;
};

}  // namespace kernel::sync

#endif  // DOOM_OS_KERNEL_SYNC_SPINLOCK_HPP_
