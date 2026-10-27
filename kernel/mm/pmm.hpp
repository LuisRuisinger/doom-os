#ifndef DOOM_OS_KERNEL_MM_PMM_HPP_
#define DOOM_OS_KERNEL_MM_PMM_HPP_

#include "kernel/boot/boot_info.hpp"
#include "kernel/core/result.hpp"
#include "kernel/core/types.hpp"
#include "kernel/init/component.hpp"
#include "kernel/mm/page.hpp"

namespace kernel::mm::pmm {

using kernel::core::paddr_t;
using kernel::core::Result;

[[nodiscard]] Result<paddr_t, mm_error> alloc(page_size size = page_size::SIZE_4K);
void free(page_size size, paddr_t base);

struct component : kernel::init::component<component, kernel::init::no_resource,
                                           kernel::boot::boot_info::component> {
    static constexpr auto *name = "PMM";

    static kernel::init::init_result init_allocator();

    template <typename View>
    static kernel::init::init_result init(View)
    {
        return init_allocator();
    }
};

}  // namespace kernel::mm::pmm

#endif  // DOOM_OS_KERNEL_MM_PMM_HPP_
