// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/memory/page.hpp"

namespace kernel::core::memory {

namespace {

page_hw_allocatability g_page_1g_hw_allocatable = page_hw_allocatability::UNINITIALIZED;

}  // namespace

// =================================================================================================
// Hardware page support
// =================================================================================================

page_hw_allocatability page<1024 * 1024 * 1024>::is_hw_allocatable()
{
    return g_page_1g_hw_allocatable;
}

page_hw_allocatability is_hw_allocatable(page_size size)
{
    switch (size) {
        case page_size::SIZE_4K:
            return page_4k::is_hw_allocatable();
        case page_size::SIZE_2M:
            return page_2m::is_hw_allocatable();
        case page_size::SIZE_1G:
            return page_1g::is_hw_allocatable();
    }

    return page_hw_allocatability::UNINITIALIZED;
}

void set_hw_allocatable(page_size size, bool allocatable)
{
    if (size != page_size::SIZE_1G)
        return;

    g_page_1g_hw_allocatable =
        allocatable ? page_hw_allocatability::YES : page_hw_allocatability::NO;
}

}  // namespace kernel::core::memory
