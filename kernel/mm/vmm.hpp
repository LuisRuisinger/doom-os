#ifndef DOOM_OS_KERNEL_MM_VMM_HPP_
#define DOOM_OS_KERNEL_MM_VMM_HPP_

#include "arch/x86_64/mmu/mmu.hpp"
#include "kernel/core/result.hpp"
#include "kernel/core/types.hpp"
#include "kernel/init/component.hpp"
#include "kernel/mm/page.hpp"

namespace kernel::mm::vmm {

using kernel::core::paddr_t;
using kernel::core::Result;
using kernel::core::u64;
using kernel::core::usize;
using kernel::core::vaddr_t;

[[nodiscard]] Result<void, mm_error> map(vaddr_t va, paddr_t pa, page_size size, page_prot prot);
[[nodiscard]] Result<void, mm_error> map_range(vaddr_t va, paddr_t pa, u64 length, page_size size,
                                               page_prot prot);
[[nodiscard]] Result<void, mm_error> protect(vaddr_t va, page_size size, page_prot prot);
[[nodiscard]] Result<void, mm_error> protect_range(vaddr_t va, u64 length, page_size size,
                                                   page_prot prot);
[[nodiscard]] Result<void, mm_error> unmap(vaddr_t va, page_size size);
[[nodiscard]] Result<void, mm_error> unmap_range(vaddr_t va, u64 length, page_size size);
[[nodiscard]] Result<mapping, mm_error> vrt_to_phy(vaddr_t va);

[[nodiscard]] inline void *phy_to_vrt(paddr_t pa)
{
    return kernel::arch::x86_64::mmu::phy_to_vrt(pa);
}

[[nodiscard]] Result<volatile void *, mm_error> map_mmio(paddr_t pa, usize bytes);

struct component : kernel::init::component<component, kernel::init::no_resource,
                                           kernel::arch::x86_64::mmu::component> {
    static constexpr auto *name = "VMM";

    template <typename View>
    static kernel::init::init_result init(View)
    {
        return kernel::core::Ok();
    }
};

}  // namespace kernel::mm::vmm

#endif  // DOOM_OS_KERNEL_MM_VMM_HPP_
