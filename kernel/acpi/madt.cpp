// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/acpi/madt.hpp"

#include "kernel/core/cast.hpp"

namespace kernel::acpi {

namespace {

madt_info m_cached_madt_info{};
bool      m_madt_parsed{false};

enum madt_type : u8 {
    TYPE_LAPIC = 0,
    TYPE_IOAPIC = 1,
    TYPE_INTERRUPT_OVERRIDE = 2,
    TYPE_NMI_SOURCE = 3,
    TYPE_LAPIC_NMI = 4,
    TYPE_LAPIC_ADDRESS_OVERRIDE = 5,
    TYPE_X2APIC = 9,
};

}  // namespace

madt_info parse_madt(sdt_view view)
{
    madt_info info{};
    if (!view.valid() || view.length() < sizeof(wire::madt_header))
        return info;

    const auto *madt = view.header as(const wire::madt_header *);
    info.lapic_address = madt->lapic_address;
    info.flags = madt->flags;

    const auto *entry_cursor = madt as(const u8 *) + sizeof(wire::madt_header);
    const auto *entry_end = madt as(const u8 *) + madt -> header.length;

    while (entry_cursor + sizeof(wire::madt_entry_header) <= entry_end) {
        const auto *header = entry_cursor as(const wire::madt_entry_header *);
        if (header->length < sizeof(wire::madt_entry_header))
            break;
        if (entry_cursor + header->length > entry_end)
            break;

        switch (header->type) {
            case TYPE_LAPIC: {
                if (header->length >= sizeof(wire::madt_lapic) &&
                    info.cpu_count < madt_info::MAX_CPUS) {
                    const auto *entry = entry_cursor as(const wire::madt_lapic *);
                    info.cpus[info.cpu_count++] = {
                        .apic_id = entry->apic_id,
                        .acpi_id = entry->acpi_processor_id,
                        .enabled = (entry->flags & 0x1) != 0,
                        .online_capable = (entry->flags & 0x2) != 0,
                    };
                }
                break;
            }

            case TYPE_IOAPIC: {
                if (header->length >= sizeof(wire::madt_ioapic) &&
                    info.ioapic_count < madt_info::MAX_IOAPICS) {
                    const auto *entry = entry_cursor as(const wire::madt_ioapic *);
                    info.ioapics[info.ioapic_count++] = {
                        .id = entry->ioapic_id,
                        .address = entry->ioapic_address as(paddr_t),
                        .gsi_base = entry->gsi_base,
                    };
                }
                break;
            }

            case TYPE_INTERRUPT_OVERRIDE: {
                if (header->length >= sizeof(wire::madt_iso) &&
                    info.iso_count < madt_info::MAX_ISOS) {
                    const auto *entry = entry_cursor as(const wire::madt_iso *);
                    info.isos[info.iso_count++] = {
                        .bus = entry->bus,
                        .source = entry->source,
                        .gsi = entry->gsi,
                        .flags = entry->flags,
                    };
                }
                break;
            }

            case TYPE_LAPIC_ADDRESS_OVERRIDE: {
                if (header->length >= sizeof(wire::madt_lapic_address_override)) {
                    const auto *entry = entry_cursor as(const wire::madt_lapic_address_override *);
                    info.lapic_address = entry->lapic_address as(paddr_t);
                }
                break;
            }

            case TYPE_X2APIC: {
                if (header->length >= sizeof(wire::madt_x2apic) &&
                    info.cpu_count < madt_info::MAX_CPUS) {
                    const auto *entry = entry_cursor as(const wire::madt_x2apic *);
                    info.cpus[info.cpu_count++] = {
                        .apic_id = entry->x2apic_id,
                        .acpi_id = entry->acpi_processor_id,
                        .enabled = (entry->flags & 0x1) != 0,
                        .online_capable = (entry->flags & 0x2) != 0,
                    };
                }
                break;
            }

            default:
                break;
        }

        entry_cursor += header->length;
    }

    m_cached_madt_info = info;
    m_madt_parsed = true;
    return info;
}

const madt_info &get_madt_info()
{
    if (!m_madt_parsed) {
        sdt_view view = find_table("APIC");
        if (view.valid()) {
            parse_madt(view);
        }
    }
    return m_cached_madt_info;
}

}  // namespace kernel::acpi