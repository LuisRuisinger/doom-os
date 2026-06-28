// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/runtime/cxx_abi.hpp"

#include "kernel/debug/kpanic.hpp"

namespace kernel::runtime {

using kernel::core::i32;
using kernel::core::u64;
using kernel::core::usize;

// =================================================================================================
// Constants
// =================================================================================================

static constexpr usize MAX_CXX_DESTRUCTORS = 256;

// =================================================================================================
// C++ destructor registry
// =================================================================================================

struct cxx_destructor_entry {
    cxx_destructor destructor{};
    void          *object{};
    void          *dso{};
    bool           active{};
};

static cxx_destructor_entry destructor_entries_m[MAX_CXX_DESTRUCTORS]{};
static usize                destructor_count_m{0};

// =================================================================================================
// C++ ABI destructor registration
// =================================================================================================

i32 register_cxx_destructor(cxx_destructor destructor, void *object, void *dso) noexcept {
    if (destructor == nullptr)
        return -1;

    if (destructor_count_m >= MAX_CXX_DESTRUCTORS)
        return -1;

    destructor_entries_m[destructor_count_m++] = cxx_destructor_entry{
        .destructor = destructor,
        .object = object,
        .dso = dso,
        .active = true,
    };

    return 0;
}

void finalize_cxx_destructors(void *dso) noexcept {
    for (usize i = destructor_count_m; i > 0; --i) {
        cxx_destructor_entry &entry = destructor_entries_m[i - 1];

        if (!entry.active)
            continue;

        if (dso != nullptr && entry.dso != dso)
            continue;

        entry.active = false;
        entry.destructor(entry.object);
    }
}

}  // namespace kernel::runtime

// =================================================================================================
// C++ ABI symbols
// =================================================================================================

extern "C" {

void *__dso_handle = nullptr;

[[noreturn]] void __cxa_pure_virtual() noexcept { KPANIC("pure virtual function called"); }

[[noreturn]] void __cxa_deleted_virtual() noexcept { KPANIC("deleted virtual function called"); }

int __cxa_atexit(void (*destructor)(void *), void *object, void *dso) noexcept {
    return kernel::runtime::register_cxx_destructor(destructor, object, dso);
}

void __cxa_finalize(void *dso) noexcept { kernel::runtime::finalize_cxx_destructors(dso); }

}  // extern "C"

// =================================================================================================
// Function-local static guard support
//
// These are only needed if we allow function-local statics without compiling with
// -fno-threadsafe-statics.
//
// Guard layout follows the usual Itanium C++ ABI idea:
//  bit 0: initialized
//  bit 1: initialization in progress
// =================================================================================================

extern "C" int __cxa_guard_acquire(kernel::core::u64 *guard) noexcept {
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

extern "C" void __cxa_guard_release(kernel::core::u64 *guard) noexcept {
    static constexpr kernel::core::u64 INITIALIZED = 1ull << 0;

    __atomic_store_n(guard, INITIALIZED, __ATOMIC_RELEASE);
}

extern "C" void __cxa_guard_abort(kernel::core::u64 *guard) noexcept {
    __atomic_store_n(guard, 0, __ATOMIC_RELEASE);
}