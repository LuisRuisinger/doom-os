// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/mm/vmm.hpp"

#include "kernel/core/bits.hpp"
#include "kernel/sync/spinlock.hpp"

namespace kernel::mm::vmm {

namespace {

using kernel::arch::x86_64::mmu::MMIO_MAP_BASE;
using kernel::arch::x86_64::mmu::MMIO_MAP_SIZE;
using kernel::core::utils::align_down;
using kernel::core::utils::align_up;

struct mmio_allocator {
    kernel::sync::spinlock lock{};
    vaddr_t                cursor{MMIO_MAP_BASE};
};

mmio_allocator m_mmio{};

}  // namespace

address_space &kernel_address_space()
{
    return kernel::arch::x86_64::mmu::kernel_address_space();
}

Result<void, vmm_error> map(vaddr_t virtual_address, paddr_t physical_address, page_size size,
                            page_flags flags)
{
    if (!kernel::arch::x86_64::mmu::map(virtual_address, physical_address, size, flags))
        return kernel::core::Err(vmm_error::MAPPING_FAILED);

    return kernel::core::Ok();
}

Result<void, vmm_error> map_range(vaddr_t virtual_address, paddr_t physical_address, u64 length,
                                  page_size size, page_flags flags)
{
    if (!kernel::arch::x86_64::mmu::map_range(virtual_address, physical_address, length, size,
                                              flags))
        return kernel::core::Err(vmm_error::MAPPING_FAILED);

    return kernel::core::Ok();
}

Result<void, vmm_error> unmap(vaddr_t virtual_address, page_size size)
{
    if (!kernel::arch::x86_64::mmu::unmap(virtual_address, size))
        return kernel::core::Err(vmm_error::UNMAPPING_FAILED);

    return kernel::core::Ok();
}

Result<mapping, vmm_error> vrt_to_phy(vaddr_t virtual_address)
{
    const mapping result = kernel::arch::x86_64::mmu::vrt_to_phy(virtual_address);

    if (!result.present)
        return kernel::core::Err(vmm_error::NOT_MAPPED);

    return kernel::core::Ok(result);
}

Result<vaddr_t, vmm_error> map_mmio(paddr_t physical_address, usize bytes)
{
    if (bytes == 0)
        return kernel::core::Err(vmm_error::INVALID_ARGUMENT);

    const paddr_t phys_base = align_down<paddr_t>(physical_address, PAGE_SIZE_4K);
    const u64     offset = physical_address - phys_base;
    const u64     length = align_up<u64>(bytes + offset, PAGE_SIZE_4K);

    vaddr_t virt_start = 0;

    {
        kernel::sync::spinlock_guard guard(m_mmio.lock);

        if (length > (MMIO_MAP_BASE + MMIO_MAP_SIZE) - m_mmio.cursor)
            return kernel::core::Err(vmm_error::OUT_OF_ADDRESS_SPACE);

        virt_start = m_mmio.cursor;
        m_mmio.cursor += length;
    }

    return map_range(virt_start, phys_base, length, page_size::SIZE_4K, flags_mmio()).map([&] {
        return virt_start + offset;
    });
}

kernel::init::init_result component::init_vmm()
{
    return kernel::core::Ok();
}

}  // namespace kernel::mm::vmm