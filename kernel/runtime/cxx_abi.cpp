
#include "kernel/core/types.hpp"
#include "kernel/debug/kpanic.hpp"

#define DOOM_OS_WEAK __attribute__((weak))

extern "C" {

[[noreturn]] DOOM_OS_WEAK void __cxa_pure_virtual() noexcept
{
    KPANIC("pure virtual function called");
}

[[noreturn]] DOOM_OS_WEAK void __cxa_deleted_virtual() noexcept
{
    KPANIC("deleted virtual function called");
}

}  // extern "C"

extern "C" DOOM_OS_WEAK int __cxa_guard_acquire(kernel::core::u64 *guard) noexcept
{
    static constexpr kernel::core::u64 INITIALIZED = 1ull << 0;
    static constexpr kernel::core::u64 IN_USE = 1ull << 1;

    if (__atomic_load_n(guard, __ATOMIC_ACQUIRE) & INITIALIZED)
        return 0;

    for (;;) {
        kernel::core::u64 expected = 0;
        if (__atomic_compare_exchange_n(guard, &expected, IN_USE, false, __ATOMIC_ACQ_REL,
                                        __ATOMIC_ACQUIRE))
            return 1;

        if (expected & INITIALIZED)
            return 0;

        asm volatile("" ::: "memory");
    }
}

extern "C" DOOM_OS_WEAK void __cxa_guard_release(kernel::core::u64 *guard) noexcept
{
    __atomic_store_n(guard, 1ull << 0, __ATOMIC_RELEASE);
}

extern "C" DOOM_OS_WEAK void __cxa_guard_abort(kernel::core::u64 *guard) noexcept
{
    __atomic_store_n(guard, 0, __ATOMIC_RELEASE);
}

#undef DOOM_OS_WEAK
