// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/acpi/madt.hpp"

namespace kernel::acpi {

namespace {

madt_info m_cached_madt_info{};
bool      m_madt_parsed{false};

enum madt_type : u8 {
    TYPE_LAPIC                  = 0,
    TYPE_IOAPIC                 = 1,
    TYPE_INTERRUPT_OVERRIDE     = 2,
    TYPE_NMI_SOURCE             = 3,
    TYPE_LAPIC_NMI              = 4,
    TYPE_LAPIC_ADDRESS_OVERRIDE = 5,
    TYPE_X2APIC                 = 9,
};

}  // namespace

madt_info parse_madt(sdt_view view)
{
    madt_info info{};
    if (!view.valid() || view.length() < sizeof(wire::madt_header))
        return info;

    const auto *madt = reinterpret_cast<const wire::madt_header *>(view.header);
    info.lapic_address = madt->lapic_address;
    info.flags         = madt->flags;

    const auto *entry_cursor = reinterpret_cast<const u8 *>(madt) + sizeof(wire::madt_header);
    const auto *entry_end    = reinterpret_cast<const u8 *>(madt) + madt->header.length;

    while (entry_cursor + sizeof(wire::madt_entry_header) <= entry_end) {
        const auto *header = reinterpret_cast<const wire::madt_entry_header *>(entry_cursor);
        if (header->length < sizeof(wire::madt_entry_header))
            break;
        if (entry_cursor + header->length > entry_end)
            break;

        switch (header->type) {
            case TYPE_LAPIC: {
                if (header->length >= sizeof(wire::madt_lapic) && info.cpu_count < madt_info::MAX_CPUS) {
                    const auto *entry = reinterpret_cast<const wire::madt_lapic *>(entry_cursor);
                    info.cpus[info.cpu_count++] = {
                        .apic_id        = entry->apic_id,
                        .acpi_id        = entry->acpi_processor_id,
                        .enabled        = (entry->flags & 0x1) != 0,
                        .online_capable = (entry->flags & 0x2) != 0,
                    };
                }
                break;
            }

            case TYPE_IOAPIC: {
                if (header->length >= sizeof(wire::madt_ioapic) && info.ioapic_count < madt_info::MAX_IOAPICS) {
                    const auto *entry = reinterpret_cast<const wire::madt_ioapic *>(entry_cursor);
                    info.ioapics[info.ioapic_count++] = {
                        .id      = entry->ioapic_id,
                        .address = static_cast<paddr_t>(entry->ioapic_address),
                        .gsi_base = entry->gsi_base,
                    };
                }
                break;
            }

            case TYPE_INTERRUPT_OVERRIDE: {
                if (header->length >= sizeof(wire::madt_iso) && info.iso_count < madt_info::MAX_ISOS) {
                    const auto *entry = reinterpret_cast<const wire::madt_iso *>(entry_cursor);
                    info.isos[info.iso_count++] = {
                        .bus    = entry->bus,
                        .source = entry->source,
                        .gsi    = entry->gsi,
                        .flags  = entry->flags,
                    };
                }
                break;
            }

            case TYPE_LAPIC_ADDRESS_OVERRIDE: {
                if (header->length >= sizeof(wire::madt_lapic_address_override)) {
                    const auto *entry = reinterpret_cast<const wire::madt_lapic_address_override *>(entry_cursor);
                    info.lapic_address = static_cast<paddr_t>(entry->lapic_address);
                }
                break;
            }

            case TYPE_X2APIC: {
                if (header->length >= sizeof(wire::madt_x2apic) && info.cpu_count < madt_info::MAX_CPUS) {
                    const auto *entry = reinterpret_cast<const wire::madt_x2apic *>(entry_cursor);
                    info.cpus[info.cpu_count++] = {
                        .apic_id        = entry->x2apic_id,
                        .acpi_id        = entry->acpi_processor_id,
                        .enabled        = (entry->flags & 0x1) != 0,
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
    m_madt_parsed      = true;
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