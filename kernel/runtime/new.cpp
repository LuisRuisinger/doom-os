// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/runtime/new.hpp"

#include "kernel/debug/kpanic.hpp"

namespace kernel::runtime {

// =================================================================================================
// Dynamic allocation stubs
// =================================================================================================

// TODO
// implement vmm
[[noreturn]] static void allocation_panic()
{
    KPANIC("dynamic allocation used before kernel heap is available");
}

// TODO
// implement vmm
[[noreturn]] static void deallocation_panic()
{
    KPANIC("dynamic deallocation used before kernel heap is available");
}

}  // namespace kernel::runtime

// =================================================================================================
// Global allocation operators
// =================================================================================================

void *operator new(kernel::core::usize size)
{
    static_cast<void>(size);

    // TODO
    // implement vmm
    kernel::runtime::allocation_panic();
}

void *operator new[](kernel::core::usize size)
{
    static_cast<void>(size);

    // TODO
    // implement vmm
    kernel::runtime::allocation_panic();
}

void operator delete(void *ptr) noexcept
{
    if (ptr == nullptr)
        return;

    // TODO
    // implement vmm
    kernel::runtime::deallocation_panic();
}

void operator delete(void *ptr, kernel::core::usize size) noexcept
{
    static_cast<void>(size);

    operator delete(ptr);
}

void operator delete[](void *ptr) noexcept
{
    if (ptr == nullptr)
        return;

    // TODO
    // implement vmm
    kernel::runtime::deallocation_panic();
}

void operator delete[](void *ptr, kernel::core::usize size) noexcept
{
    static_cast<void>(size);

    operator delete[](ptr);
}