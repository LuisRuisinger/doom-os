// =================================================================================================
// Kernel files
// =================================================================================================

#include "gdt.hpp"

#include "../cpu/cpu.hpp"

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

static constexpr descriptor make_tss_descriptor_low(u64 base, u32 limit) {
    return descriptor{
        .limit_low = static_cast<u16>(limit & 0xFFFF),
        .base_low = static_cast<u16>(base & 0xFFFF),
        .base_mid = static_cast<u8>((base >> 16) & 0xFF),
        .access = ACCESS_PRESENT | ACCESS_RING_0 | ACCESS_TSS_AVAILABLE_64,
        .limit_high_flags = static_cast<u8>((limit >> 16) & 0x0F),
        .base_high = static_cast<u8>((base >> 24) & 0xFF),
    };
}

static constexpr descriptor make_tss_descriptor_high(u64 base) {
    const u32 base_high = static_cast<u32>(base >> 32);

    return descriptor{
        .limit_low = static_cast<u16>(base_high & 0xFFFF),
        .base_low = static_cast<u16>((base_high >> 16) & 0xFFFF),
        .base_mid = 0,
        .access = 0,
        .limit_high_flags = 0,
        .base_high = 0,
    };
}

// =================================================================================================
// Table
// =================================================================================================

void table::init(const kernel::arch::x86_64::tss::state &task_state_segment) {
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
        reinterpret_cast<kernel::core::u64>(&entries_m[0]),
    };
}

void table::load() const { gdt_load(&ptr_m, KERNEL_CODE_SELECTOR, KERNEL_DATA_SELECTOR); }

// =================================================================================================
// GDT
// =================================================================================================

void init_table(table &target, const kernel::arch::x86_64::tss::state &task_state_segment) {
    target.init(task_state_segment);
}

void load_table(const table &target) { target.load(); }

void load_task_register(u16 selector) { asm volatile("ltr %0" : : "r"(selector) : "memory"); }

// =================================================================================================
// Core component
// =================================================================================================

bool core_component::init_component(kernel::arch::x86_64::cpu::local_state &cpu) {
    init_table(cpu.gdt, cpu.task_state_segment);
    load_table(cpu.gdt);
    load_task_register(TSS_SELECTOR);

    return true;
}

}  // namespace kernel::arch::x86_64::gdt
