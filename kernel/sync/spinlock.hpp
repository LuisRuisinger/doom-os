#ifndef DOOM_OS_KERNEL_SYNC_SPINLOCK_HPP_
#define DOOM_OS_KERNEL_SYNC_SPINLOCK_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/cpu.hpp"
#include "kernel/core/types.hpp"
#include "kernel/sync/atomic.hpp"

namespace kernel::sync {

using kernel::core::u8;

// =================================================================================================
// Constants
// =================================================================================================

static constexpr u8 SPINLOCK_UNLOCKED = 0;
static constexpr u8 SPINLOCK_LOCKED = 1;

// =================================================================================================
// Spinlock
// =================================================================================================

class spinlock {
    atomic<u8> state_m{SPINLOCK_UNLOCKED};

   public:
    constexpr spinlock() = default;

    spinlock(const spinlock &) = delete;
    spinlock &operator=(const spinlock &) = delete;

    spinlock(spinlock &&) = delete;
    spinlock &operator=(spinlock &&) = delete;

    void lock() {
        for (;;) {
            if (try_lock())
                return;

            while (state_m.load(memory_order::RELAXED) == SPINLOCK_LOCKED)
                kernel::arch::x64::cpu::relax();
        }
    }

    bool try_lock() {
        u8 expected = SPINLOCK_UNLOCKED;
        return state_m.compare_exchange(expected, SPINLOCK_LOCKED, memory_order::ACQUIRE,
                                        memory_order::RELAXED);
    }

    void unlock() { state_m.store(SPINLOCK_UNLOCKED, memory_order::RELEASE); }

    bool is_locked() const { return state_m.load(memory_order::RELAXED) == SPINLOCK_LOCKED; }
};

// =================================================================================================
// Spinlock guard
// =================================================================================================

class spinlock_guard {
    spinlock &lock_m;

   public:
    explicit spinlock_guard(spinlock &lock) : lock_m(lock) { lock_m.lock(); }

    ~spinlock_guard() { lock_m.unlock(); }

    spinlock_guard(const spinlock_guard &) = delete;
    spinlock_guard &operator=(const spinlock_guard &) = delete;

    spinlock_guard(spinlock_guard &&) = delete;
    spinlock_guard &operator=(spinlock_guard &&) = delete;
};

}  // namespace kernel::sync

#endif  // DOOM_OS_KERNEL_SYNC_SPINLOCK_HPP_