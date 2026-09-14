#ifndef DOOM_OS_KERNEL_MM_KHEAP_HPP_
#define DOOM_OS_KERNEL_MM_KHEAP_HPP_

#include "kernel/core/types.hpp"
#include "kernel/init/component.hpp"
#include "kernel/mm/vmm.hpp"

namespace kernel::mm::kheap {

using kernel::core::usize;

[[nodiscard]] void *alloc(usize bytes, usize alignment = 2 * sizeof(void *));
void free(void *ptr);

struct component
    : kernel::init::component<component, kernel::init::no_resource, kernel::mm::vmm::component> {
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
