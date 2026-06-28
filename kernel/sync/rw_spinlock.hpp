#ifndef DOOM_OS_KERNEL_SYNC_RW_SPINLOCK_HPP_
#define DOOM_OS_KERNEL_SYNC_RW_SPINLOCK_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/cpu.hpp"
#include "kernel/core/types.hpp"
#include "kernel/sync/atomic.hpp"

namespace kernel::sync {

using kernel::core::u32;

// =================================================================================================
// Constants
// =================================================================================================

static constexpr u32 RW_SPINLOCK_WRITER_ACTIVE = 1u << 31;
static constexpr u32 RW_SPINLOCK_WRITER_PENDING = 1u << 30;
static constexpr u32 RW_SPINLOCK_READER_MASK =
    ~(RW_SPINLOCK_WRITER_ACTIVE | RW_SPINLOCK_WRITER_PENDING);

static constexpr u32 RW_SPINLOCK_READ_BLOCKED =
    RW_SPINLOCK_WRITER_ACTIVE | RW_SPINLOCK_WRITER_PENDING;

static constexpr u32 RW_SPINLOCK_READER_ONE = 1;

// =================================================================================================
// Reader-writer spinlock
// =================================================================================================

class rw_spinlock {
    atomic<u32> state_m{0};

   public:
    constexpr rw_spinlock() = default;

    rw_spinlock(const rw_spinlock &) = delete;
    rw_spinlock &operator=(const rw_spinlock &) = delete;

    rw_spinlock(rw_spinlock &&) = delete;
    rw_spinlock &operator=(rw_spinlock &&) = delete;

    void read_lock() {
        for (;;) {
            u32 state = state_m.load(memory_order::RELAXED);

            if (state & RW_SPINLOCK_READ_BLOCKED) {
                while (state_m.load(memory_order::RELAXED) & RW_SPINLOCK_READ_BLOCKED)
                    kernel::arch::x86_64::cpu::relax();

                continue;
            }

            if ((state & RW_SPINLOCK_READER_MASK) == RW_SPINLOCK_READER_MASK) {
                kernel::arch::x86_64::cpu::relax();
                continue;
            }

            u32 desired = state + RW_SPINLOCK_READER_ONE;
            if (state_m.compare_exchange(state, desired, memory_order::ACQUIRE,
                                         memory_order::RELAXED))
                return;
        }
    }

    bool try_read_lock() {
        u32 state = state_m.load(memory_order::RELAXED);
        if (state & RW_SPINLOCK_READ_BLOCKED)
            return false;

        if ((state & RW_SPINLOCK_READER_MASK) == RW_SPINLOCK_READER_MASK)
            return false;

        u32 desired = state + RW_SPINLOCK_READER_ONE;
        return state_m.compare_exchange(state, desired, memory_order::ACQUIRE,
                                        memory_order::RELAXED);
    }

    void read_unlock() { state_m.fetch_sub(RW_SPINLOCK_READER_ONE, memory_order::RELEASE); }

    void write_lock() {
        for (;;) {
            u32 state = state_m.load(memory_order::RELAXED);

            if ((state & RW_SPINLOCK_WRITER_PENDING) == 0) {
                u32 expected = state;
                u32 desired = state | RW_SPINLOCK_WRITER_PENDING;

                if (!state_m.compare_exchange(expected, desired, memory_order::RELAXED,
                                              memory_order::RELAXED)) {
                    kernel::arch::x86_64::cpu::relax();
                    continue;
                }
            }

            for (;;) {
                u32 expected = RW_SPINLOCK_WRITER_PENDING;

                if (state_m.compare_exchange(expected, RW_SPINLOCK_WRITER_ACTIVE,
                                             memory_order::ACQUIRE, memory_order::RELAXED))
                    return;

                if ((expected & RW_SPINLOCK_WRITER_PENDING) == 0)
                    break;

                kernel::arch::x86_64::cpu::relax();
            }
        }
    }

    bool try_write_lock() {
        u32 expected = 0;
        return state_m.compare_exchange(expected, RW_SPINLOCK_WRITER_ACTIVE, memory_order::ACQUIRE,
                                        memory_order::RELAXED);
    }

    void write_unlock() {
        for (;;) {
            u32 state = state_m.load(memory_order::RELAXED);
            u32 desired = 0;

            if (state & RW_SPINLOCK_WRITER_PENDING)
                desired = RW_SPINLOCK_WRITER_PENDING;

            if (state_m.compare_exchange(state, desired, memory_order::RELEASE,
                                         memory_order::RELAXED))
                return;
        }
    }

    bool is_write_locked() const {
        return state_m.load(memory_order::RELAXED) & RW_SPINLOCK_WRITER_ACTIVE;
    }

    bool has_pending_writer() const {
        return state_m.load(memory_order::RELAXED) & RW_SPINLOCK_WRITER_PENDING;
    }

    bool has_readers() const { return reader_count() != 0; }

    u32 reader_count() const {
        return state_m.load(memory_order::RELAXED) & RW_SPINLOCK_READER_MASK;
    }

    bool is_locked() const { return state_m.load(memory_order::RELAXED) != 0; }
};

// =================================================================================================
// Read guard
// =================================================================================================

class read_spinlock_guard {
    rw_spinlock &lock_m;

   public:
    explicit read_spinlock_guard(rw_spinlock &lock) : lock_m(lock) { lock_m.read_lock(); }

    ~read_spinlock_guard() { lock_m.read_unlock(); }

    read_spinlock_guard(const read_spinlock_guard &) = delete;
    read_spinlock_guard &operator=(const read_spinlock_guard &) = delete;

    read_spinlock_guard(read_spinlock_guard &&) = delete;
    read_spinlock_guard &operator=(read_spinlock_guard &&) = delete;
};

// =================================================================================================
// Write guard
// =================================================================================================

class write_spinlock_guard {
    rw_spinlock &lock_m;

   public:
    explicit write_spinlock_guard(rw_spinlock &lock) : lock_m(lock) { lock_m.write_lock(); }

    ~write_spinlock_guard() { lock_m.write_unlock(); }

    write_spinlock_guard(const write_spinlock_guard &) = delete;
    write_spinlock_guard &operator=(const write_spinlock_guard &) = delete;

    write_spinlock_guard(write_spinlock_guard &&) = delete;
    write_spinlock_guard &operator=(write_spinlock_guard &&) = delete;
};

// =================================================================================================
// Try read guard
// =================================================================================================

class try_read_spinlock_guard {
    rw_spinlock &lock_m;
    bool         locked_m{};

   public:
    explicit try_read_spinlock_guard(rw_spinlock &lock)
        : lock_m(lock), locked_m(lock_m.try_read_lock()) {}

    ~try_read_spinlock_guard() {
        if (locked_m)
            lock_m.read_unlock();
    }

    try_read_spinlock_guard(const try_read_spinlock_guard &) = delete;
    try_read_spinlock_guard &operator=(const try_read_spinlock_guard &) = delete;

    try_read_spinlock_guard(try_read_spinlock_guard &&) = delete;
    try_read_spinlock_guard &operator=(try_read_spinlock_guard &&) = delete;

    bool locked() const { return locked_m; }

    explicit operator bool() const { return locked_m; }
};

// =================================================================================================
// Try write guard
// =================================================================================================

class try_write_spinlock_guard {
    rw_spinlock &lock_m;
    bool         locked_m{};

   public:
    explicit try_write_spinlock_guard(rw_spinlock &lock)
        : lock_m(lock), locked_m(lock_m.try_write_lock()) {}

    ~try_write_spinlock_guard() {
        if (locked_m)
            lock_m.write_unlock();
    }

    try_write_spinlock_guard(const try_write_spinlock_guard &) = delete;
    try_write_spinlock_guard &operator=(const try_write_spinlock_guard &) = delete;

    try_write_spinlock_guard(try_write_spinlock_guard &&) = delete;
    try_write_spinlock_guard &operator=(try_write_spinlock_guard &&) = delete;

    bool locked() const { return locked_m; }

    explicit operator bool() const { return locked_m; }
};

}  // namespace kernel::sync

#endif  // DOOM_OS_KERNEL_SYNC_RW_SPINLOCK_HPP_