// =================================================================================================
// Kernel files
// =================================================================================================

#include "arch/x86_64/gdt/gdt.hpp"

#include "kernel/core/cast.hpp"

namespace kernel::arch::x86_64::gdt {
using kernel::core::u16;
using kernel::core::u32;
using kernel::core::u8;

// =================================================================================================
// Assembly
// =================================================================================================

extern "C" void gdt_load(const void *gdt_pointer, u16 code_selector, u16 data_selector);

// =================================================================================================
// Descriptor constants
// =================================================================================================

static_assert(sizeof(descriptor) == 8);
static_assert(sizeof(pointer) == 10);

static constexpr u8 ACCESS_PRESENT = 0x80;
static constexpr u8 ACCESS_RING_0 = 0x00;
static constexpr u8 ACCESS_RING_3 = 0x60;
static constexpr u8 ACCESS_DESCRIPTOR = 0x10;
static constexpr u8 ACCESS_EXECUTABLE = 0x08;
static constexpr u8 ACCESS_READ_WRITE = 0x02;
static constexpr u8 ACCESS_TSS_AVAILABLE_64 = 0x09;

static constexpr u8 FLAGS_GRANULARITY_4K = 0x80;
static constexpr u8 FLAGS_32_BIT = 0x40;
static constexpr u8 FLAGS_64_BIT = 0x20;

static constexpr u32 FLAT_BASE = 0;
static constexpr u32 FLAT_LIMIT = 0xFFFFF;

static constexpr u16 TSS_ENTRY_INDEX = TSS_SELECTOR >> 3;

// =================================================================================================
// Descriptor construction
// =================================================================================================

static constexpr descriptor make_descriptor(u32 base, u32 limit, u8 access, u8 flags)
{
    return descriptor{
        .limit_low = (limit & 0xFFFF) as(u16),
        .base_low = (base & 0xFFFF) as(u16),
        .base_mid = ((base >> 16) & 0xFF) as(u8),
        .access = access,
        .limit_high_flags = (((limit >> 16) & 0x0F) | (flags & 0xF0)) as(u8),
        .base_high = ((base >> 24) & 0xFF) as(u8),
    };
}

static constexpr descriptor make_tss_descriptor_low(u64 base, u32 limit)
{
    return descriptor{
        .limit_low = (limit & 0xFFFF) as(u16),
        .base_low = (base & 0xFFFF) as(u16),
        .base_mid = ((base >> 16) & 0xFF) as(u8),
        .access = ACCESS_PRESENT | ACCESS_RING_0 | ACCESS_TSS_AVAILABLE_64,
        .limit_high_flags = ((limit >> 16) & 0x0F) as(u8),
        .base_high = ((base >> 24) & 0xFF) as(u8),
    };
}

static constexpr descriptor make_tss_descriptor_high(u64 base)
{
    const u32 base_high = (base >> 32) as(u32);

    return descriptor{
        .limit_low = (base_high & 0xFFFF) as(u16),
        .base_low = ((base_high >> 16) & 0xFFFF) as(u16),
        .base_mid = 0,
        .access = 0,
        .limit_high_flags = 0,
        .base_high = 0,
    };
}

// =================================================================================================
// Table
// =================================================================================================

void table::init(const kernel::arch::x86_64::tss::state &task_state_segment)
{
    static_assert(TSS_ENTRY_INDEX == 5);
    static_assert(TSS_ENTRY_INDEX + 1 < ENTRY_COUNT);

    for (auto &i : entries_m)
        i = make_descriptor(0, 0, 0, 0);

    // GDT base
    entries_m[0] = make_descriptor(0, 0, 0, 0);

    entries_m[1] = make_descriptor(
        FLAT_BASE, FLAT_LIMIT,
        ACCESS_PRESENT | ACCESS_RING_0 | ACCESS_DESCRIPTOR | ACCESS_EXECUTABLE | ACCESS_READ_WRITE,
        FLAGS_GRANULARITY_4K | FLAGS_64_BIT);

    entries_m[2] =
        make_descriptor(FLAT_BASE, FLAT_LIMIT,
                        ACCESS_PRESENT | ACCESS_RING_0 | ACCESS_DESCRIPTOR | ACCESS_READ_WRITE,
                        FLAGS_GRANULARITY_4K | FLAGS_32_BIT);

    entries_m[3] =
        make_descriptor(FLAT_BASE, FLAT_LIMIT,
                        ACCESS_PRESENT | ACCESS_RING_3 | ACCESS_DESCRIPTOR | ACCESS_READ_WRITE,
                        FLAGS_GRANULARITY_4K | FLAGS_32_BIT);

    entries_m[4] = make_descriptor(
        FLAT_BASE, FLAT_LIMIT,
        ACCESS_PRESENT | ACCESS_RING_3 | ACCESS_DESCRIPTOR | ACCESS_EXECUTABLE | ACCESS_READ_WRITE,
        FLAGS_GRANULARITY_4K | FLAGS_64_BIT);

    // TSS descriptor
    entries_m[TSS_ENTRY_INDEX] =
        make_tss_descriptor_low(task_state_segment.base(), tss::state::limit());
    entries_m[TSS_ENTRY_INDEX + 1] = make_tss_descriptor_high(task_state_segment.base());

    ptr_m = pointer{
        sizeof(entries_m) - 1,
        (&entries_m[0]) as(kernel::core::u64),
    };
}

void table::load() const
{
    gdt_load(&ptr_m, KERNEL_CODE_SELECTOR, KERNEL_DATA_SELECTOR);
}

u16 table::kernel_code_selector() const
{
    return KERNEL_CODE_SELECTOR;
}

// =================================================================================================
// GDT
// =================================================================================================

void load_task_register(u16 selector)
{
    asm volatile("ltr %0" : : "r"(selector) : "memory");
}

}  // namespace kernel::arch::x86_64::gdt
