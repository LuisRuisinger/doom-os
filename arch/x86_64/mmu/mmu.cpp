// =================================================================================================
// Config files
// =================================================================================================

#include "config/layout.h"

// =================================================================================================
// Kernel files
// =================================================================================================

#include "arch/x86_64/cpu/registers.hpp"
#include "kernel/boot/boot_info.hpp"
#include "kernel/core/bits.hpp"
#include "arch/x86_64/mmu/entry.hpp"
#include "arch/x86_64/mmu/environment.hpp"
#include "arch/x86_64/mmu/features.hpp"
#include "arch/x86_64/mmu/mmu.hpp"
#include "arch/x86_64/mmu/tlb.hpp"
#include "kernel/sync/spinlock.hpp"

namespace kernel::arch::x86_64::mmu {

namespace {

using kernel::init::init_error;
using kernel::core::uptr;
using kernel::core::usize;
using kernel::core::utils::align_down;
using kernel::core::utils::align_up;
using kernel::core::utils::is_aligned;

inline constexpr usize EARLY_PD_ENTRY_COUNT = static_cast<usize>(EARLY_MAP_SIZE / PAGE_SIZE_2M);
inline constexpr usize DIRECT_PD_COUNT = static_cast<usize>(DIRECT_MAP_SIZE / PAGE_SIZE_1G);

static_assert(EARLY_MAP_SIZE % PAGE_SIZE_2M == 0);
static_assert(EARLY_PD_ENTRY_COUNT <= ENTRIES_PER_TABLE);
static_assert(DIRECT_PD_COUNT > 0);
static_assert(DIRECT_PD_COUNT <= ENTRIES_PER_TABLE);
static_assert(pml4_index(KERNEL_BASE) != pml4_index(DIRECT_MAP_BASE));
static_assert(pml4_index(DIRECT_MAP_BASE) != 0);

// =================================================================================================
// Linker symbols
// =================================================================================================

extern "C" char kernel_end[];
extern "C" char kernel_physical_end[];
extern "C" char kernel_physical_start[];
extern "C" char __text_start[];
extern "C" char __text_end[];
extern "C" char __rodata_start[];
extern "C" char __rodata_end[];
extern "C" char __data_rel_ro_start[];

// =================================================================================================
// Bootstrap tables
//
// These are part of the kernel image, so PMM reserves them before the VMM starts. They give us a
// safe first permanent address space with an identity map, a direct map, and the higher-half
// kernel mapping that is refined once CR3 has changed.
//
// They are static rather than allocated because they have to exist before the direct map does,
// and the PMM cannot hand out a frame that is reachable that early. Being inside the image is
// what makes them addressable through KERNEL_BASE while they are being filled in.
//
// m_identity_pd is load-bearing and cannot be dropped as a boot leftover: longmode.S leaves RSP
// on boot_stack_top in .boot.bss, a low physical address, and nothing ever moves it. The running
// kernel stack is reachable only through the identity map. Removing it triple-faults on the next
// push, with no diagnostic.
// =================================================================================================

page_table m_bootstrap_pml4{};
page_table m_identity_pdpt{};
page_table m_identity_pd{};
page_table m_kernel_pdpt{};
page_table m_kernel_pd{};
page_table m_direct_pdpt{};
page_table m_direct_pds[DIRECT_PD_COUNT]{};

address_space          m_kernel_space{};
kernel::sync::spinlock m_kernel_space_lock{};

void enable_write_protect()
{
    namespace regs = kernel::arch::x86_64::cpu;

    regs::write_cr0(regs::read_cr0() | regs::CR0_WRITE_PROTECT);
}

[[nodiscard]] paddr_t kernel_physical_address(const void *address)
{
    return static_cast<paddr_t>(reinterpret_cast<uptr>(address) - KERNEL_BASE);
}

[[nodiscard]] vaddr_t symbol_address(const char *symbol)
{
    return static_cast<vaddr_t>(reinterpret_cast<uptr>(symbol));
}

[[nodiscard]] bool map_range_no_flush(vaddr_t virtual_address, paddr_t physical_address, u64 length,
                                      page_size size, page_flags flags)
{
    const u64 step = bytes_in(size);

    if (length == 0)
        return true;

    if (!is_aligned<vaddr_t>(virtual_address, step) ||
        !is_aligned<paddr_t>(physical_address, step) || length % step != 0)
        return false;

    for (u64 offset = 0; offset < length; offset += step)
        if (!m_kernel_space.map(virtual_address + offset, physical_address + offset, size, flags)) {
            // Removes what this call added, so a failure does not leave a half-mapped range
            // behind. It does not restore mappings that were replaced on the way through -
            // recording those to put them back is a bigger contract than any caller here wants.
            for (u64 done = 0; done < offset; done += step)
                (void)m_kernel_space.unmap(virtual_address + done, size);

            return false;
        }

    return true;
}

[[nodiscard]] bool unmap_range_no_flush(vaddr_t virtual_address, u64 length, page_size size)
{
    const u64 step = bytes_in(size);

    for (u64 offset = 0; offset < length; offset += step)
        if (!m_kernel_space.unmap(virtual_address + offset, size))
            return false;

    return true;
}

// =================================================================================================
// Bootstrap
//
// The tables are pre-linked by hand and the leaves are then filled through address_space::map,
// so the entry encoding has exactly one implementation. Pre-linking is what keeps map() from
// needing alloc_table(), which cannot run until the direct map exists.
// =================================================================================================

void clear_bootstrap_tables()
{
    m_bootstrap_pml4.clear();
    m_identity_pdpt.clear();
    m_identity_pd.clear();
    m_kernel_pdpt.clear();
    m_kernel_pd.clear();
    m_direct_pdpt.clear();

    for (usize i = 0; i < DIRECT_PD_COUNT; ++i)
        m_direct_pds[i].clear();
}

void link_bootstrap_tables()
{
    m_bootstrap_pml4[0] = table_entry(kernel_physical_address(&m_identity_pdpt), false);
    m_identity_pdpt[0] = table_entry(kernel_physical_address(&m_identity_pd), false);

    m_bootstrap_pml4[pml4_index(KERNEL_BASE)] =
        table_entry(kernel_physical_address(&m_kernel_pdpt), false);
    m_kernel_pdpt[pdpt_index(KERNEL_BASE)] =
        table_entry(kernel_physical_address(&m_kernel_pd), false);

    m_bootstrap_pml4[pml4_index(DIRECT_MAP_BASE)] =
        table_entry(kernel_physical_address(&m_direct_pdpt), false);

    for (usize i = 0; i < DIRECT_PD_COUNT; ++i)
        m_direct_pdpt[i] = table_entry(kernel_physical_address(&m_direct_pds[i]), false);
}

// How much of the higher half the bootstrap covers. Only the kernel image is linked there, so
// mapping the full early window would leave every 2 MiB leaf past the image mapped writable and
// executable over whatever physical memory happens to sit underneath - including the frames the
// PMM is about to hand out.
[[nodiscard]] u64 kernel_window_size()
{
    return align_up<u64>(static_cast<u64>(reinterpret_cast<uptr>(kernel_physical_end)),
                         PAGE_SIZE_2M);
}

// The direct map covers described memory only. Filling it from the physical ceiling instead maps
// every hole in the address space, and gives each MMIO aperture below the ceiling - the LAPIC,
// PCI BARs, the framebuffer - a second, writeback-cached, writable alias.
[[nodiscard]] bool map_direct_window(const kernel::boot::info &boot, page_flags flags)
{
    for (usize i = 0; i < boot.memory_map.count; ++i) {
        const auto &region = boot.memory_map[i];

        if (region.kind == kernel::boot::memory_kind::DEFECTIVE || region.length == 0 ||
            region.base >= DIRECT_MAP_SIZE)
            continue;

        const u64 available = DIRECT_MAP_SIZE - region.base;
        const u64 length = region.length > available ? available : region.length;

        const paddr_t first = align_down<paddr_t>(region.base, PAGE_SIZE_2M);
        const paddr_t last = align_up<paddr_t>(region.base + length, PAGE_SIZE_2M);

        // Regions that share a 2 MiB granule map it more than once, with the same frame and the
        // same flags, so the repeat is a no-op rather than a conflict.
        if (!map_range_no_flush(DIRECT_MAP_BASE + first, first, last - first, page_size::SIZE_2M,
                                flags))
            return false;
    }

    return true;
}

[[nodiscard]] bool build_bootstrap_tables(const kernel::boot::info &boot)
{
    page_flags identity_flags{};
    page_flags kernel_flags{};
    page_flags direct_flags{};

    identity_flags.word(0) = PAGE_FLAG_WRITABLE | PAGE_FLAG_NO_EXECUTE;
    kernel_flags.word(0) = PAGE_FLAG_WRITABLE | PAGE_FLAG_GLOBAL;
    direct_flags.word(0) = PAGE_FLAG_WRITABLE | PAGE_FLAG_GLOBAL | PAGE_FLAG_NO_EXECUTE;

    clear_bootstrap_tables();
    link_bootstrap_tables();

    m_kernel_space.set_root(kernel_physical_address(&m_bootstrap_pml4));

    if (!map_range_no_flush(0, 0, EARLY_MAP_SIZE, page_size::SIZE_2M, identity_flags))
        return false;

    if (!map_range_no_flush(KERNEL_BASE, 0, kernel_window_size(), page_size::SIZE_2M, kernel_flags))
        return false;

    return map_direct_window(boot, direct_flags);
}

[[nodiscard]] bool map_kernel_region(char *first, char *last, page_flags flags)
{
    const vaddr_t start = align_down<vaddr_t>(symbol_address(first), PAGE_SIZE_4K);
    const vaddr_t end = align_up<vaddr_t>(symbol_address(last), PAGE_SIZE_4K);

    if (end <= start)
        return true;

    // Runs after the CR3 switch, so the walk is available and answers what the linker
    // arithmetic used to: this window maps physical 0 at KERNEL_BASE. Unlike that subtraction
    // the walk can come back empty, and re-mapping the kernel image onto physical 0 is not a
    // failure worth surviving.
    const mapping resolved = m_kernel_space.translate(start);

    if (!resolved.present)
        return false;

    return map_range_no_flush(start, resolved.physical(), end - start, page_size::SIZE_4K, flags);
}

[[nodiscard]] bool protect_kernel_image()
{
    page_flags writable_data{};
    page_flags text{};
    page_flags rodata{};

    writable_data.word(0) = PAGE_FLAG_WRITABLE | PAGE_FLAG_GLOBAL | PAGE_FLAG_NO_EXECUTE;
    text.word(0) = PAGE_FLAG_GLOBAL;
    rodata.word(0) = PAGE_FLAG_GLOBAL | PAGE_FLAG_NO_EXECUTE;

    if (!map_kernel_region(__data_rel_ro_start, kernel_end, writable_data))
        return false;

    if (!map_kernel_region(__rodata_start, __rodata_end, rodata))
        return false;

    if (!map_kernel_region(__text_start, __text_end, text))
        return false;

    return true;
}

// protect_kernel_image only refines the three section ranges. Everything else inside the
// higher-half window is still whatever the bootstrap left there - 4 KiB entries that inherited
// writable+executable when the covering 2 MiB leaf was split. Nothing is linked there, so the
// honest state is not-present rather than permissive.
[[nodiscard]] bool seal_kernel_window()
{
    const vaddr_t image_first = align_down<vaddr_t>(symbol_address(__text_start), PAGE_SIZE_4K);
    const vaddr_t image_last = align_up<vaddr_t>(symbol_address(kernel_end), PAGE_SIZE_4K);
    const vaddr_t window_last = KERNEL_BASE + kernel_window_size();

    if (!unmap_range_no_flush(KERNEL_BASE, image_first - KERNEL_BASE, page_size::SIZE_4K))
        return false;

    return unmap_range_no_flush(image_last, window_last - image_last, page_size::SIZE_4K);
}

}  // namespace

// =================================================================================================
// Active kernel address space
// =================================================================================================

address_space &kernel_address_space()
{
    return m_kernel_space;
}

init_result init_kernel_address_space()
{
    namespace regs = kernel::arch::x86_64::cpu;

    const kernel::boot::info &boot = kernel::boot::boot_info::current();

    if (!boot.valid)
        return kernel::core::Err(init_error::INVALID_BOOT_DATA);

    enable_paging_features();

    // Before anything can unmap, so a collected table is never mistaken for a PMM frame.
    set_reserved_table_range(static_cast<paddr_t>(reinterpret_cast<uptr>(kernel_physical_start)),
                             static_cast<paddr_t>(reinterpret_cast<uptr>(kernel_physical_end)));

    // Nothing here allocates - the tables are static and pre-linked - so a failure means the
    // layout constants disagree with each other, not that the machine is short of memory.
    if (!build_bootstrap_tables(boot))
        return kernel::core::Err(init_error::UNSPECIFIED);

    regs::write_cr3(m_kernel_space.root());
    mark_direct_map_ready();

    // These split 2 MiB leaves into page tables, which is the first thing here that needs frames
    // from the PMM, so a failure really is a memory shortage.
    if (!protect_kernel_image() || !seal_kernel_window())
        return kernel::core::Err(init_error::NO_USABLE_MEMORY);

    tlb::flush_all_global();
    enable_write_protect();

    return kernel::core::Ok();
}

bool map(vaddr_t virtual_address, paddr_t physical_address, page_size size, page_flags flags)
{
    kernel::sync::spinlock_guard guard(m_kernel_space_lock);

    if (!m_kernel_space.map(virtual_address, physical_address, size, flags))
        return false;

    tlb::flush(virtual_address);
    return true;
}

bool map_range(vaddr_t virtual_address, paddr_t physical_address, u64 length, page_size size,
               page_flags flags)
{
    kernel::sync::spinlock_guard guard(m_kernel_space_lock);

    if (!map_range_no_flush(virtual_address, physical_address, length, size, flags))
        return false;

    if ((flags.word(0) & PAGE_FLAG_GLOBAL) != 0 && global_pages_enabled())
        tlb::flush_all_global();
    else
        tlb::flush_all();

    return true;
}

bool unmap(vaddr_t virtual_address, page_size size)
{
    kernel::sync::spinlock_guard guard(m_kernel_space_lock);

    if (!m_kernel_space.unmap(virtual_address, size))
        return false;

    tlb::flush(virtual_address);
    return true;
}

mapping vrt_to_phy(vaddr_t virtual_address)
{
    kernel::sync::spinlock_guard guard(m_kernel_space_lock);

    return m_kernel_space.translate(virtual_address);
}

}  // namespace kernel::arch::x86_64::mmu
