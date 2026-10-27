#include "arch/x86_64/cpu/cpu.hpp"
#include "kernel/core/types.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/sync/atomic.hpp"

#define DOOM_OS_WEAK __attribute__((weak))

namespace {

using kernel::core::u64;
using kernel::sync::atomic_ref;
using kernel::sync::memory_order;

constexpr u64 INITIALIZED = u64{1} << 0;
constexpr u64 IN_USE = u64{1} << 8;

}  // namespace

extern "C" {
[[noreturn]] DOOM_OS_WEAK void __cxa_pure_virtual() noexcept
{
    KPANIC("pure virtual function called");
}

[[noreturn]] DOOM_OS_WEAK void __cxa_deleted_virtual() noexcept
{
    KPANIC("deleted virtual function called");
}

DOOM_OS_WEAK int __cxa_guard_acquire(u64 *guard) noexcept
{
    const atomic_ref state(*guard);
    if ((state.load(memory_order::ACQUIRE) & INITIALIZED) != 0)
        return 0;

    for (;;) {
        u64 expected = 0;
        if (state.compare_exchange(expected, IN_USE, memory_order::ACQ_REL, memory_order::ACQUIRE))
            return 1;

        if ((expected & INITIALIZED) != 0)
            return 0;

        kernel::arch::x86_64::cpu::relax();
    }
}

DOOM_OS_WEAK void __cxa_guard_release(u64 *guard) noexcept
{
    atomic_ref(*guard).store(INITIALIZED, memory_order::RELEASE);
}

DOOM_OS_WEAK void __cxa_guard_abort(u64 *guard) noexcept
{
    atomic_ref(*guard).store(0, memory_order::RELEASE);
}

}  // extern "C"

#undef DOOM_OS_WEAK
