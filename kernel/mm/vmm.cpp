#include "kernel/mm/vmm.hpp"

#include "kernel/core/array.hpp"
#include "kernel/core/bits.hpp"
#include "kernel/core/cast.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/mm/pmm.hpp"
#include "kernel/sync/spinlock.hpp"

namespace kernel::mm::vmm {

namespace {

namespace mmu = kernel::arch::x86_64::mmu;

using kernel::core::Err;
using kernel::core::Ok;
using kernel::core::u64;
using kernel::core::utils::align_down;
using kernel::core::utils::align_up;
using kernel::core::utils::array;

constexpr usize MAX_ALLOCATIONS = 1024;

struct allocation {
    vaddr_t base;
    u64     bytes;
};

kernel::sync::spinlock             m_lock{};
array<allocation, MAX_ALLOCATIONS> m_allocations{};
usize                              m_count = 0;

[[nodiscard]] vaddr_t gap_start(usize index)
{
    if (index == 0)
        return mmu::VMALLOC_BASE;

    const allocation &previous = m_allocations[index - 1];

    return previous.base + previous.bytes + FRAME_SIZE;
}

[[nodiscard]] vaddr_t gap_end(usize index)
{
    return index == m_count ? mmu::VMALLOC_BASE + mmu::VMALLOC_SIZE : m_allocations[index].base;
}

[[nodiscard]] Result<usize, mm_error> find_gap(u64 bytes)
{
    if (m_count == MAX_ALLOCATIONS)
        return Err(mm_error::OUT_OF_MEMORY);

    for (usize index = 0; index <= m_count; ++index)
        if (gap_end(index) - gap_start(index) >= bytes + FRAME_SIZE)
            return Ok(index);

    return Err(mm_error::OUT_OF_MEMORY);
}

void insert(usize index, allocation entry)
{
    for (usize i = m_count; i > index; --i)
        m_allocations[i] = m_allocations[i - 1];

    m_allocations[index] = entry;
    ++m_count;
}

void erase(usize index)
{
    for (usize i = index + 1; i < m_count; ++i)
        m_allocations[i - 1] = m_allocations[i];

    --m_count;
}

void release(vaddr_t base, u64 bytes)
{
    if (bytes == 0)
        return;

    const auto unmapped = mmu::kernel_space().unmap_range(
        base, bytes, page_size::SIZE_4K,
        [](const mapping &removed) { pmm::free(removed.size, removed.frame); });

    if (unmapped.is_err())
        KPANIC("vmm: allocation at {:#018X} is not fully mapped", base);
}

[[nodiscard]] Result<void, mm_error> back(vaddr_t base, u64 bytes, page_prot prot)
{
    for (u64 offset = 0; offset < bytes; offset += FRAME_SIZE) {
        const auto frame = pmm::alloc();

        if (frame.is_err()) {
            release(base, offset);
            return Err(frame.unwrap_err_ref());
        }

        const auto mapped = mmu::kernel_space().map_range(base + offset, frame.unwrap_ref(),
                                                          FRAME_SIZE, page_size::SIZE_4K, prot);

        if (mapped.is_err()) {
            pmm::free(page_size::SIZE_4K, frame.unwrap_ref());
            release(base, offset);
            return Err(mapped.unwrap_err_ref());
        }
    }

    return Ok();
}

}  // namespace

Result<void *, mm_error> alloc(usize bytes, page_prot prot)
{
    if (bytes == 0 || bytes > mmu::VMALLOC_SIZE)
        return Err(mm_error::INVALID_ADDRESS);

    const u64 size = align_up<u64>(bytes, FRAME_SIZE);

    kernel::sync::spinlock_guard guard(m_lock);

    const usize   index = KTRY(find_gap(size));
    const vaddr_t base = gap_start(index);

    KTRY(back(base, size, prot));
    insert(index, {base, size});

    return Ok(base as(void *));
}

Result<void, mm_error> free(void *ptr, usize bytes)
{
    const vaddr_t base = ptr as(vaddr_t);

    kernel::sync::spinlock_guard guard(m_lock);

    for (usize index = 0; index < m_count; ++index) {
        const allocation &entry = m_allocations[index];

        if (entry.base != base)
            continue;

        if (entry.bytes != align_up<u64>(bytes, FRAME_SIZE))
            return Err(mm_error::INVALID_ADDRESS);

        release(entry.base, entry.bytes);
        erase(index);

        return Ok();
    }

    return Err(mm_error::INVALID_ADDRESS);
}

Result<void, mm_error> protect(void *ptr, usize bytes, page_prot prot)
{
    const vaddr_t first = ptr as(vaddr_t);
    const vaddr_t             last = align_up<vaddr_t>(first + bytes, FRAME_SIZE);

    if (bytes == 0 || (first & (FRAME_SIZE - 1)) != 0 || last <= first)
        return Err(mm_error::INVALID_ADDRESS);

    kernel::sync::spinlock_guard guard(m_lock);

    for (usize index = 0; index < m_count; ++index) {
        const allocation &entry = m_allocations[index];

        if (first >= entry.base && last <= entry.base + entry.bytes)
            return mmu::kernel_space().protect_range(first, last - first, page_size::SIZE_4K, prot);
    }

    return Err(mm_error::INVALID_ADDRESS);
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

    {
        kernel::sync::spinlock_guard guard(m_lock);

        KTRY(mmu::kernel_space().map_range(mmu::MMIO_MAP_BASE + first, first, last - first,
                                           page_size::SIZE_4K, page_prot::MMIO));
    }

    return Ok((mmu::MMIO_MAP_BASE + pa) as(volatile void *));
}

}  // namespace kernel::mm::vmm
