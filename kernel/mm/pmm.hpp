#ifndef DOOM_OS_KERNEL_MM_PMM_HPP_
#define DOOM_OS_KERNEL_MM_PMM_HPP_

#include "config/layout.h"
#include "kernel/boot/boot_info.hpp"
#include "kernel/core/result.hpp"
#include "kernel/core/types.hpp"
#include "kernel/init/component.hpp"
#include "kernel/mm/page.hpp"

namespace kernel::mm::pmm {

using kernel::core::paddr_t;
using kernel::core::Result;
using kernel::core::u64;
using kernel::core::usize;

inline constexpr u64   MAX_PHYSICAL_MEMORY = DOOM_OS_MAX_PHYSICAL_MEMORY;
inline constexpr usize MAX_FRAMES = MAX_PHYSICAL_MEMORY / FRAME_SIZE;

static_assert(MAX_PHYSICAL_MEMORY % bytes_in(page_size::SIZE_1G) == 0);

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
