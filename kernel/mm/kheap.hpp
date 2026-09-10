#ifndef DOOM_OS_KERNEL_MM_KHEAP_HPP_
#define DOOM_OS_KERNEL_MM_KHEAP_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"
#include "kernel/init/component.hpp"
#include "kernel/mm/vmm.hpp"

namespace kernel::mm::kheap {

using kernel::core::usize;

// =================================================================================================
// Kernel heap
//
// The sub-page allocator, sitting on the PMM the way the PMM sits on the memory map. It exists
// because the PMM's unit is a frame: a caller wanting 48 bytes would otherwise take 4 KiB, and two
// hundred such callers would take 800 KiB to hold 9.
//
// alloc returns nullptr on failure rather than panicking. Whether an allocation may fail is the
// caller's question - this layer has no way to tell one it may refuse from one it may not, which
// is the same reason global operator new is absent (kernel/runtime/cxx_abi.cpp).
//
// The largest single allocation is one 2 MiB page, less dlmalloc's own headers. The heap grows a
// page at a time, so a request that will not fit in a fresh one has nowhere to go and returns
// nullptr - measured, not assumed. That is a ceiling on chunks of kernel bookkeeping, which is all
// this layer is for; a caller that wants a block that size wants frames, and pmm is the layer that
// deals in them.
//
// There is one heap, shared by the kernel and by uk::services::alloc. Drivers are trusted and run
// in the same address space, so a second space would buy accounting rather than isolation, and
// isolation only with a footprint limit on top. See dlmalloc_config.h for what a split would cost.
// =================================================================================================

[[nodiscard]] void *alloc(usize bytes, usize alignment);
void free(void *ptr);

// =================================================================================================
// Component
//
// Depends on the VMM rather than on the MMU directly: the heap reaches its frames through the
// direct map, which only answers once the VMM has initialized and CR3 has switched.
// =================================================================================================

struct component : kernel::init::component<component, kernel::init::no_resource,
                                           kernel::mm::vmm::component> {
    static constexpr auto *name = "KHEAP";

    static kernel::init::init_result init_heap();

    template <typename View>
    static kernel::init::init_result init(View)
    {
        return init_heap();
    }
};

}  // namespace kernel::mm::kheap

#endif  // DOOM_OS_KERNEL_MM_KHEAP_HPP_