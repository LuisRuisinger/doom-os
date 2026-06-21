// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/gdt.hpp"

#include "kernel/core/types.hpp"

namespace kernel::arch::x86_64::gdt {

using kernel::core::u8;
using kernel::core::u16;
using kernel::core::u32;
using kernel::core::u64;

// =================================================================================================
// External assembly
// =================================================================================================

extern "C" void gdt_load(const void* gdt_pointer, u16 code_selector, u16 data_selector);

// =================================================================================================
// GDT structures
// =================================================================================================

struct [[gnu::packed]] descriptor {
    u16 limit_low;
    u16 base_low;
    u8 base_mid;
    u8 access;
    u8 limit_high_flags;
    u8 base_high;
};

struct [[gnu::packed]] pointer {
    u16 limit;
    u64 base;
};

static_assert(sizeof(descriptor) == 8);
static_assert(sizeof(pointer) == 10);

// =================================================================================================
// GDT flags
// =================================================================================================

static constexpr u8 ACCESS_PRESENT    = 0x80;
static constexpr u8 ACCESS_RING_0     = 0x00;
static constexpr u8 ACCESS_RING_3     = 0x60;
static constexpr u8 ACCESS_DESCRIPTOR = 0x10;
static constexpr u8 ACCESS_EXECUTABLE = 0x08;
static constexpr u8 ACCESS_READ_WRITE = 0x02;

static constexpr u8 FLAGS_GRANULARITY_4K = 0x80;
static constexpr u8 FLAGS_32_BIT         = 0x40;
static constexpr u8 FLAGS_64_BIT         = 0x20;

static constexpr u32 FLAT_BASE = 0;
static constexpr u32 FLAT_LIMIT = 0xFFFFF;

// =================================================================================================
// Descriptor construction
// =================================================================================================

static constexpr descriptor make_descriptor(u32 base, u32 limit, u8 access, u8 flags) {
    return descriptor{
        .limit_low = static_cast<u16>(limit & 0xFFFF),
        .base_low = static_cast<u16>(base & 0xFFFF),
        .base_mid = static_cast<u8>((base >> 16) & 0xFF),
        .access = access,
        .limit_high_flags = static_cast<u8>(((limit >> 16) & 0x0F) | (flags & 0xF0)),
        .base_high = static_cast<u8>((base >> 24) & 0xFF),
    };
}

// =================================================================================================
// GDT storage
// =================================================================================================

alignas(8) static descriptor gdt_entries[] = {
    make_descriptor(0, 0, 0, 0),

    make_descriptor(
        FLAT_BASE,
        FLAT_LIMIT,
        ACCESS_PRESENT | ACCESS_RING_0 | ACCESS_DESCRIPTOR | ACCESS_EXECUTABLE | ACCESS_READ_WRITE,
        FLAGS_GRANULARITY_4K | FLAGS_64_BIT
    ),

    make_descriptor(
        FLAT_BASE,
        FLAT_LIMIT,
        ACCESS_PRESENT | ACCESS_RING_0 | ACCESS_DESCRIPTOR | ACCESS_READ_WRITE,
        FLAGS_GRANULARITY_4K | FLAGS_32_BIT
    ),

    make_descriptor(
        FLAT_BASE,
        FLAT_LIMIT,
        ACCESS_PRESENT | ACCESS_RING_3 | ACCESS_DESCRIPTOR | ACCESS_READ_WRITE,
        FLAGS_GRANULARITY_4K | FLAGS_32_BIT
    ),

    make_descriptor(
        FLAT_BASE,
        FLAT_LIMIT,
        ACCESS_PRESENT | ACCESS_RING_3 | ACCESS_DESCRIPTOR | ACCESS_EXECUTABLE | ACCESS_READ_WRITE,
        FLAGS_GRANULARITY_4K | FLAGS_64_BIT
    ),
};

static pointer gdt_pointer{
    .limit = static_cast<u16>(sizeof(gdt_entries) - 1),
    .base = reinterpret_cast<u64>(&gdt_entries[0]),
};

// =================================================================================================
// Public API
// =================================================================================================

void init() {
    gdt_load(&gdt_pointer, KERNEL_CODE_SELECTOR, KERNEL_DATA_SELECTOR);
}

} // namespace kernel::arch::x86_64::gdt