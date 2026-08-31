// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"
#include "kernel/debug/kpanic.hpp"

// =================================================================================================
// C++ ABI
//
// What the kernel's own C++ needs from a runtime, so that needing it does not depend on what the
// application linked. A virtual call and a function-local static are language features, not
// application features; leaving them to libsupc++ would make them available only in images that
// happen to carry a C++ application, which is not a property kernel code can reason about.
//
// Weak, and that word is doing exactly one job: an application object carrying its own definitions
// - a Rust staticlib, or a C++ application that linked a runtime into itself - overrides these
// rather than colliding with them. It does not defer to libsupc++, which lives in an archive: a
// weak definition already satisfies the reference, so the archive member is never extracted. That
// is the intent. These bodies panic with a message and pull no unwinder behind them, which is what
// an image compiled -fno-exceptions throughout wants from a failure it cannot recover from.
//
// Absent, and staying absent: operator new. The kernel does not allocate through it - global
// operator new has no parameter for which pool, whether this may sleep, or what happens on
// failure - and a weak definition here would shadow libsupc++'s for the application too.
// =================================================================================================

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

// =================================================================================================
// Function-local static guards
//
// Reachable from kernel code only when it is compiled without -fno-threadsafe-statics, which it is
// not, and from prebuilt libstdc++ objects, which are. Kept so that dropping that flag stays a
// decision rather than a link error.
//
// Guard layout follows the usual Itanium C++ ABI idea:
//  bit 0: initialized
//  bit 1: initialization in progress
// =================================================================================================

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
