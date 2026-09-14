#include "kernel/mm/vmm.hpp"

#include "kernel/core/bits.hpp"
#include "kernel/core/cast.hpp"
#include "kernel/sync/spinlock.hpp"

namespace kernel::mm::vmm {

namespace {

namespace mmu = kernel::arch::x86_64::mmu;

using kernel::core::Err;
using kernel::core::Ok;
using kernel::core::utils::align_down;
using kernel::core::utils::align_up;

kernel::sync::spinlock m_lock{};

}  // namespace

Result<void, mm_error> map(vaddr_t va, paddr_t pa, page_size size, page_prot prot)
{
    kernel::sync::spinlock_guard guard(m_lock);

    return mmu::kernel_space().map(va, pa, size, prot);
}

Result<void, mm_error> map_range(vaddr_t va, paddr_t pa, u64 length, page_size size, page_prot prot)
{
    kernel::sync::spinlock_guard guard(m_lock);

    return mmu::kernel_space().map_range(va, pa, length, size, prot);
}

Result<void, mm_error> protect(vaddr_t va, page_size size, page_prot prot)
{
    kernel::sync::spinlock_guard guard(m_lock);

    return mmu::kernel_space().protect(va, size, prot);
}

Result<void, mm_error> protect_range(vaddr_t va, u64 length, page_size size, page_prot prot)
{
    kernel::sync::spinlock_guard guard(m_lock);

    return mmu::kernel_space().protect_range(va, length, size, prot);
}

Result<void, mm_error> unmap(vaddr_t va, page_size size)
{
    kernel::sync::spinlock_guard guard(m_lock);

    return mmu::kernel_space().unmap(va, size);
}

Result<void, mm_error> unmap_range(vaddr_t va, u64 length, page_size size)
{
    kernel::sync::spinlock_guard guard(m_lock);

    return mmu::kernel_space().unmap_range(va, length, size);
}

Result<mapping, mm_error> vrt_to_phy(vaddr_t va)
{
    kernel::sync::spinlock_guard guard(m_lock);

    return mmu::kernel_space().translate(va);
}

Result<volatile void *, mm_error> map_mmio(paddr_t pa, usize bytes)
{
    if (bytes == 0 || pa >= mmu::MMIO_MAP_SIZE || bytes > mmu::MMIO_MAP_SIZE - pa)
        return Err(mm_error::INVALID_ADDRESS);

    const paddr_t first = align_down<paddr_t>(pa, FRAME_SIZE);
    const paddr_t last = align_up<paddr_t>(pa + bytes, FRAME_SIZE);

    KTRY(map_range(mmu::MMIO_MAP_BASE + first, first, last - first, page_size::SIZE_4K,
                   page_prot::MMIO));

    return Ok((mmu::MMIO_MAP_BASE + pa) as(volatile void *));
}

}  // namespace kernel::mm::vmm
