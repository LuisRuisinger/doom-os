// =================================================================================================
// Config files
// =================================================================================================

#include "config/layout.h"

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/cpu/registers.hpp"
#include "kernel/core/bits.hpp"
#include "kernel/core/memory/vmm/mmu/entry.hpp"
#include "kernel/core/memory/vmm/mmu/mmu.hpp"
#include "kernel/core/memory/vmm/mmu/tlb.hpp"

namespace kernel::core::memory::vmm::mmu {

namespace {

using kernel::core::init_error;
using kernel::core::uptr;
using kernel::core::usize;
using kernel::core::utils::align_down;
using kernel::core::utils::align_up;
using kernel::core::utils::is_aligned;

inline constexpr usize EARLY_PD_ENTRY_COUNT =
    static_cast<usize>(static_cast<u64>(DOOM_OS_EARLY_MAP_SIZE) / PAGE_SIZE_2M);
inline constexpr usize DIRECT_PD_COUNT = static_cast<usize>(DIRECT_MAP_SIZE / PAGE_SIZE_1G);

inline constexpr u64 EARLY_MAP_SIZE = static_cast<u64>(DOOM_OS_EARLY_MAP_SIZE);

static_assert(static_cast<u64>(DOOM_OS_EARLY_MAP_SIZE) % PAGE_SIZE_2M == 0);
static_assert(EARLY_PD_ENTRY_COUNT <= ENTRIES_PER_TABLE);
static_assert(DIRECT_PD_COUNT > 0);
static_assert(DIRECT_PD_COUNT <= ENTRIES_PER_TABLE);
static_assert(pml4_index(KERNEL_BASE) != pml4_index(DIRECT_MAP_BASE));
static_assert(pml4_index(DIRECT_MAP_BASE) != 0);

// =================================================================================================
// Linker symbols
// =================================================================================================

extern "C" char kernel_end[];
extern "C" char __text_start[];
extern "C" char __text_end[];
extern "C" char __rodata_start[];
extern "C" char __rodata_end[];
extern "C" char __data_rel_ro_start[];

// =================================================================================================
// Bootstrap tables
//
// These are part of the kernel image, so PMM reserves them before the VMM starts. They give us a
// safe first permanent address space with an identity map for the boot stack, a full direct map,
// and the higher-half kernel mapping that is refined after CR3 changes.
//
// They are static rather than allocated because they have to exist before the direct map does,
// and the PMM cannot hand out a frame that is reachable that early. Being inside the image is
// what makes them addressable through KERNEL_BASE while they are being filled in.
// =================================================================================================

page_table g_bootstrap_pml4{};
page_table g_identity_pdpt{};
page_table g_identity_pd{};
page_table g_kernel_pdpt{};
page_table g_kernel_pd{};
page_table g_direct_pdpt{};
page_table g_direct_pds[DIRECT_PD_COUNT]{};

address_space g_kernel_space{};

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

[[nodiscard]] paddr_t kernel_virtual_to_physical(vaddr_t virtual_address)
{
    return static_cast<paddr_t>(virtual_address - KERNEL_BASE);
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
        if (!g_kernel_space.map(virtual_address + offset, physical_address + offset, size, flags))
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
    g_bootstrap_pml4.clear();
    g_identity_pdpt.clear();
    g_identity_pd.clear();
    g_kernel_pdpt.clear();
    g_kernel_pd.clear();
    g_direct_pdpt.clear();

    for (usize i = 0; i < DIRECT_PD_COUNT; ++i)
        g_direct_pds[i].clear();
}

void link_bootstrap_tables()
{
    g_bootstrap_pml4[0] = table_entry(kernel_physical_address(&g_identity_pdpt), false);
    g_identity_pdpt[0] = table_entry(kernel_physical_address(&g_identity_pd), false);

    g_bootstrap_pml4[pml4_index(KERNEL_BASE)] =
        table_entry(kernel_physical_address(&g_kernel_pdpt), false);
    g_kernel_pdpt[pdpt_index(KERNEL_BASE)] =
        table_entry(kernel_physical_address(&g_kernel_pd), false);

    g_bootstrap_pml4[pml4_index(DIRECT_MAP_BASE)] =
        table_entry(kernel_physical_address(&g_direct_pdpt), false);

    for (usize i = 0; i < DIRECT_PD_COUNT; ++i)
        g_direct_pdpt[i] = table_entry(kernel_physical_address(&g_direct_pds[i]), false);
}

[[nodiscard]] bool build_bootstrap_tables()
{
    const page_flags identity_flags{
        .writable = true,
        .executable = false,
        .user = false,
        .global = false,
    };
    const page_flags kernel_flags{
        .writable = true,
        .executable = true,
        .user = false,
        .global = true,
    };
    const page_flags direct_flags{
        .writable = true,
        .executable = false,
        .user = false,
        .global = true,
    };

    clear_bootstrap_tables();
    link_bootstrap_tables();

    g_kernel_space.set_root(kernel_physical_address(&g_bootstrap_pml4));

    if (!map_range_no_flush(0, 0, EARLY_MAP_SIZE, page_size::SIZE_2M, identity_flags))
        return false;

    if (!map_range_no_flush(KERNEL_BASE, 0, EARLY_MAP_SIZE, page_size::SIZE_2M, kernel_flags))
        return false;

    return map_range_no_flush(DIRECT_MAP_BASE, 0, DIRECT_MAP_SIZE, page_size::SIZE_2M,
                              direct_flags);
}

[[nodiscard]] bool map_kernel_region(char *first, char *last, page_flags flags)
{
    const vaddr_t start = align_down<vaddr_t>(symbol_address(first), PAGE_SIZE_4K);
    const vaddr_t end = align_up<vaddr_t>(symbol_address(last), PAGE_SIZE_4K);

    if (end <= start)
        return true;

    return map_range_no_flush(start, kernel_virtual_to_physical(start), end - start,
                              page_size::SIZE_4K, flags);
}

[[nodiscard]] bool protect_kernel_image()
{
    const page_flags writable_data{
        .writable = true,
        .executable = false,
        .user = false,
        .global = true,
    };
    const page_flags text{
        .writable = false,
        .executable = true,
        .user = false,
        .global = true,
    };
    const page_flags rodata{
        .writable = false,
        .executable = false,
        .user = false,
        .global = true,
    };

    if (!map_kernel_region(__data_rel_ro_start, kernel_end, writable_data))
        return false;

    if (!map_kernel_region(__rodata_start, __rodata_end, rodata))
        return false;

    if (!map_kernel_region(__text_start, __text_end, text))
        return false;

    return true;
}

}  // namespace

// =================================================================================================
// Active kernel address space
// =================================================================================================

address_space &kernel_address_space()
{
    return g_kernel_space;
}

init_result init_kernel_address_space()
{
    namespace regs = kernel::arch::x86_64::cpu;

    enable_paging_features();

    if (!build_bootstrap_tables())
        return kernel::core::Err(init_error::NO_USABLE_MEMORY);

    regs::write_cr3(g_kernel_space.root());
    mark_direct_map_ready();

    if (!protect_kernel_image())
        return kernel::core::Err(init_error::NO_USABLE_MEMORY);

    tlb::flush_all_global();
    enable_write_protect();

    return kernel::core::Ok();
}

bool map(vaddr_t virtual_address, paddr_t physical_address, page_size size, page_flags flags)
{
    if (!g_kernel_space.map(virtual_address, physical_address, size, flags))
        return false;

    tlb::flush(virtual_address);
    return true;
}

bool map_range(vaddr_t virtual_address, paddr_t physical_address, u64 length, page_size size,
               page_flags flags)
{
    if (!map_range_no_flush(virtual_address, physical_address, length, size, flags))
        return false;

    if (flags.global && features().global)
        tlb::flush_all_global();
    else
        tlb::flush_all();

    return true;
}

bool unmap(vaddr_t virtual_address, page_size size)
{
    if (!g_kernel_space.unmap(virtual_address, size))
        return false;

    tlb::flush(virtual_address);
    return true;
}

mapping translate(vaddr_t virtual_address)
{
    return g_kernel_space.translate(virtual_address);
}

}  // namespace kernel::core::memory::vmm::mmu
