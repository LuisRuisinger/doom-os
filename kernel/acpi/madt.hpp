#ifndef DOOM_OS_KERNEL_ACPI_MADT_HPP_
#define DOOM_OS_KERNEL_ACPI_MADT_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/acpi/acpi.hpp"
#include "kernel/core/types.hpp"

namespace kernel::acpi {

namespace wire {

struct [[gnu::packed]] madt_header {
    sdt_header header;
    u32        lapic_address;
    u32        flags;
};

struct [[gnu::packed]] madt_entry_header {
    u8 type;
    u8 length;
};

struct [[gnu::packed]] madt_lapic {
    madt_entry_header header;
    u8                acpi_processor_id;
    u8                apic_id;
    u32               flags;
};

struct [[gnu::packed]] madt_ioapic {
    madt_entry_header header;
    u8                ioapic_id;
    u8                reserved;
    u32               ioapic_address;
    u32               gsi_base;
};

struct [[gnu::packed]] madt_iso {
    madt_entry_header header;
    u8                bus;
    u8                source;
    u32               gsi;
    u16               flags;
};

struct [[gnu::packed]] madt_nmi {
    madt_entry_header header;
    u8                acpi_processor_id;
    u16               flags;
    u8                lint;
};

struct [[gnu::packed]] madt_lapic_address_override {
    madt_entry_header header;
    u16               reserved;
    u64               lapic_address;
};

struct [[gnu::packed]] madt_x2apic {
    madt_entry_header header;
    u16               reserved;
    u32               x2apic_id;
    u32               flags;
    u32               acpi_processor_id;
};

}  // namespace wire

struct processor_info {
    u32  apic_id{};
    u32  acpi_id{};
    bool enabled{};
    bool online_capable{};
};

struct ioapic_record {
    u8      id{};
    paddr_t address{};
    u32     gsi_base{};
};

struct iso_record {
    u8  bus{};
    u8  source{};
    u32 gsi{};
    u16 flags{};
};

struct madt_info {
    paddr_t lapic_address{0xFEE00000};
    u32     flags{};

    static constexpr usize MAX_CPUS    = 64;
    static constexpr usize MAX_IOAPICS = 8;
    static constexpr usize MAX_ISOS    = 32;

    processor_info cpus[MAX_CPUS]{};
    usize          cpu_count{};

    ioapic_record  ioapics[MAX_IOAPICS]{};
    usize          ioapic_count{};

    iso_record     isos[MAX_ISOS]{};
    usize          iso_count{};
};

[[nodiscard]] madt_info parse_madt(sdt_view view);
[[nodiscard]] const madt_info &get_madt_info();

}  // namespace kernel::acpi

#endif  // DOOM_OS_KERNEL_ACPI_MADT_HPP_