#include "arch/x86_64/mmu/mmu.hpp"

#include "arch/x86_64/cpu/registers.hpp"
#include "kernel/boot/boot_info.hpp"
#include "kernel/core/array.hpp"
#include "kernel/core/bits.hpp"
#include "kernel/core/cast.hpp"

namespace kernel::arch::x86_64::mmu {

namespace {

using kernel::core::Err;
using kernel::core::Ok;
using kernel::core::uptr;
using kernel::core::utils::align_down;
using kernel::core::utils::align_up;
using kernel::core::utils::array;
using kernel::init::init_error;

constexpr u64   PAGE_4K = bytes_in(page_size::SIZE_4K);
constexpr u64   PAGE_2M = bytes_in(page_size::SIZE_2M);
constexpr u64   PAGE_1G = bytes_in(page_size::SIZE_1G);
constexpr u64   PML4_SPAN = u64{1} << 39;
constexpr usize DIRECT_PD_COUNT = DIRECT_MAP_SIZE / PAGE_1G;

static_assert(EARLY_MAP_SIZE % PAGE_2M == 0 && EARLY_MAP_SIZE <= PAGE_1G);
static_assert(KERNEL_BASE % PAGE_1G == 0);
static_assert(DIRECT_MAP_BASE % PML4_SPAN == 0 && DIRECT_MAP_SIZE % PAGE_1G == 0);
static_assert(DIRECT_PD_COUNT <= paging::ENTRIES);
static_assert(MMIO_MAP_BASE % PML4_SPAN == 0 && MMIO_MAP_SIZE <= PML4_SPAN);
static_assert(MMIO_MAP_BASE >= DIRECT_MAP_BASE + PML4_SPAN);
static_assert(paging::index(4, KERNEL_BASE) != paging::index(4, DIRECT_MAP_BASE));

struct alignas(PAGE_4K) table {
    array<u64, paging::ENTRIES> entries;
};

table                         m_pml4{};
table                         m_identity_pdpt{};
table                         m_identity_pd{};
table                         m_kernel_pdpt{};
table                         m_kernel_pd{};
table                         m_direct_pdpt{};
array<table, DIRECT_PD_COUNT> m_direct_pds{};

address_space m_kernel_space{};

extern "C" char kernel_end[];
extern "C" char __text_start[];
extern "C" char __text_end[];
extern "C" char __rodata_start[];
extern "C" char __rodata_end[];
extern "C" char __data_rel_ro_start[];

[[nodiscard]] paddr_t image_phys(const void *address)
{
    return address as(uptr) - KERNEL_BASE;
}

[[nodiscard]] vaddr_t symbol(const char *address)
{
    return address as(uptr);
}

[[nodiscard]] u64 kernel_window_size()
{
    return align_up<u64>(kernel_physical_end as(uptr), PAGE_2M);
}

void link_tables()
{
    m_pml4.entries[0] = paging::table_entry(image_phys(&m_identity_pdpt));
    m_identity_pdpt.entries[0] = paging::table_entry(image_phys(&m_identity_pd));

    m_pml4.entries[paging::index(4, KERNEL_BASE)] = paging::table_entry(image_phys(&m_kernel_pdpt));
    m_kernel_pdpt.entries[paging::index(3, KERNEL_BASE)] =
        paging::table_entry(image_phys(&m_kernel_pd));

    m_pml4.entries[paging::index(4, DIRECT_MAP_BASE)] =
        paging::table_entry(image_phys(&m_direct_pdpt));

    for (usize i = 0; i < DIRECT_PD_COUNT; ++i)
        m_direct_pdpt.entries[i] = paging::table_entry(image_phys(&m_direct_pds[i]));
}

void fill_2m(table &pd, vaddr_t va, paddr_t pa, u64 length, page_prot prot)
{
    for (u64 offset = 0; offset < length; offset += PAGE_2M)
        pd.entries[paging::index(2, va + offset)] =
            paging::leaf_entry(pa + offset, page_size::SIZE_2M, prot);
}

void fill_direct_map(const kernel::boot::info &boot)
{
    for (const auto &region : boot.memory_map) {
        if (region.kind == kernel::boot::memory_kind::DEFECTIVE || region.length == 0 ||
            region.base >= DIRECT_MAP_SIZE)
            continue;

        const paddr_t end = region.length > DIRECT_MAP_SIZE - region.base
                                ? DIRECT_MAP_SIZE
                                : region.base + region.length;
        const paddr_t first = align_down<paddr_t>(region.base, PAGE_2M);
        const paddr_t last = align_up<paddr_t>(end, PAGE_2M);

        for (paddr_t pa = first; pa < last; pa += PAGE_2M)
            fill_2m(m_direct_pds[pa / PAGE_1G], DIRECT_MAP_BASE + pa, pa, PAGE_2M,
                    page_prot::KERNEL_DATA);
    }
}

[[nodiscard]] Result<void, mm_error> protect_region(const char *first, const char *last,
                                                    page_prot prot)
{
    const vaddr_t start = align_down<vaddr_t>(symbol(first), PAGE_4K);
    const vaddr_t end = align_up<vaddr_t>(symbol(last), PAGE_4K);

    return m_kernel_space.protect_range(start, end - start, page_size::SIZE_4K, prot);
}

[[nodiscard]] Result<void, mm_error> protect_image()
{
    KTRY(protect_region(__data_rel_ro_start, kernel_end, page_prot::KERNEL_DATA));
    KTRY(protect_region(__rodata_start, __rodata_end, page_prot::KERNEL_RODATA));
    KTRY(protect_region(__text_start, __text_end, page_prot::KERNEL_TEXT));

    const vaddr_t image_first = align_down<vaddr_t>(symbol(__text_start), PAGE_4K);
    const vaddr_t image_last = align_up<vaddr_t>(symbol(kernel_end), PAGE_4K);
    const vaddr_t window_last = KERNEL_BASE + kernel_window_size();

    KTRY(m_kernel_space.unmap_range(KERNEL_BASE, image_first - KERNEL_BASE, page_size::SIZE_4K));

    return m_kernel_space.unmap_range(image_last, window_last - image_last, page_size::SIZE_4K);
}

}  // namespace

address_space &kernel_space()
{
    return m_kernel_space;
}

kernel::init::init_result component::init_kernel_space()
{
    namespace cpu = kernel::arch::x86_64::cpu;

    const kernel::boot::info &boot = kernel::boot::boot_info::current();

    if (!boot.valid)
        return Err(init_error::INVALID_BOOT_DATA);

    enable_paging_features();
    link_tables();

    fill_2m(m_identity_pd, 0, 0, EARLY_MAP_SIZE, page_prot::WRITE);
    fill_2m(m_kernel_pd, KERNEL_BASE, 0, kernel_window_size(),
            page_prot::WRITE | page_prot::EXEC | page_prot::GLOBAL);
    fill_direct_map(boot);

    m_kernel_space = address_space{image_phys(&m_pml4)};
    cpu::write_cr3(m_kernel_space.root());

    if (protect_image().is_err())
        return Err(init_error::NO_USABLE_MEMORY);

    tlb::flush_all_global();
    cpu::write_cr0(cpu::read_cr0() | cpu::CR0_WRITE_PROTECT);

    return Ok();
}

}  // namespace kernel::arch::x86_64::mmu
