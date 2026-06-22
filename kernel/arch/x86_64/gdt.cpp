// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/gdt.hpp"

#include "kernel/arch/x86_64/cpu.hpp"

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

static constexpr u8 FLAGS_GRANULARITY_4K = 0x80;
static constexpr u8 FLAGS_32_BIT = 0x40;
static constexpr u8 FLAGS_64_BIT = 0x20;

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
// Table
// =================================================================================================

void table::init() {
    for (u16 i = 0; i < ENTRY_COUNT; ++i) {
        entries_[i] = make_descriptor(0, 0, 0, 0);
    }

    entries_[0] = make_descriptor(0, 0, 0, 0);

    entries_[1] = make_descriptor(
        FLAT_BASE, FLAT_LIMIT,
        ACCESS_PRESENT | ACCESS_RING_0 | ACCESS_DESCRIPTOR | ACCESS_EXECUTABLE | ACCESS_READ_WRITE,
        FLAGS_GRANULARITY_4K | FLAGS_64_BIT);

    entries_[2] =
        make_descriptor(FLAT_BASE, FLAT_LIMIT,
                        ACCESS_PRESENT | ACCESS_RING_0 | ACCESS_DESCRIPTOR | ACCESS_READ_WRITE,
                        FLAGS_GRANULARITY_4K | FLAGS_32_BIT);

    entries_[3] =
        make_descriptor(FLAT_BASE, FLAT_LIMIT,
                        ACCESS_PRESENT | ACCESS_RING_3 | ACCESS_DESCRIPTOR | ACCESS_READ_WRITE,
                        FLAGS_GRANULARITY_4K | FLAGS_32_BIT);

    entries_[4] = make_descriptor(
        FLAT_BASE, FLAT_LIMIT,
        ACCESS_PRESENT | ACCESS_RING_3 | ACCESS_DESCRIPTOR | ACCESS_EXECUTABLE | ACCESS_READ_WRITE,
        FLAGS_GRANULARITY_4K | FLAGS_64_BIT);

    /*
     * entries_[5] and entries_[6] are reserved for the future 64-bit TSS descriptor.
     * A 64-bit TSS descriptor consumes two GDT slots.
     */

    pointer_ = pointer{
        .limit = static_cast<u16>(sizeof(entries_) - 1),
        .base = reinterpret_cast<kernel::core::u64>(&entries_[0]),
    };
}

void table::load() const { gdt_load(&pointer_, KERNEL_CODE_SELECTOR, KERNEL_DATA_SELECTOR); }

// =================================================================================================
// GDT
// =================================================================================================

void init_table(table &target) { target.init(); }

void load_table(const table &target) { target.load(); }

// =================================================================================================
// Core component
// =================================================================================================

bool core_component::init_component(kernel::arch::x86_64::cpu::local_state &cpu) {
    init_table(cpu.gdt);
    load_table(cpu.gdt);
    return true;
}
}  // namespace kernel::arch::x86_64::gdt