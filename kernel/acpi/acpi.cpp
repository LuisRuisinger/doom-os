// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/acpi/acpi.hpp"

#include "kernel/boot/boot_info.hpp"
#include "kernel/mm/vmm.hpp"

namespace kernel::acpi {

namespace {

paddr_t m_rsdp_phys{};
paddr_t m_sdt_root_phys{};
bool    m_is_xsdt{false};
bool    m_initialized{false};

[[nodiscard]] bool signature_match(const char a[4], const char b[4])
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

}  // namespace

bool validate_checksum(const void *pointer, usize length)
{
    if (!pointer || length == 0)
        return false;

    const auto *bytes = static_cast<const u8 *>(pointer);
    u8 sum = 0;
    for (usize i = 0; i < length; ++i) {
        sum = static_cast<u8>(sum + bytes[i]);
    }
    return sum == 0;
}

void init(paddr_t rsdp_address)
{
    m_rsdp_phys = rsdp_address;
    m_sdt_root_phys = 0;
    m_is_xsdt = false;
    m_initialized = false;

    if (m_rsdp_phys == 0)
        return;

    const auto *rsdp = static_cast<const wire::rsdp_descriptor *>(
        kernel::mm::vmm::phy_to_vrt(m_rsdp_phys)
    );

    if (!rsdp)
        return;

    for (usize i = 0; i < 8; ++i) {
        if (rsdp->signature[i] != "RSD PTR "[i])
            return;
    }

    if (!validate_checksum(rsdp, 20))
        return;

    if (rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
        if (!validate_checksum(rsdp, rsdp->length))
            return;
        m_sdt_root_phys = static_cast<paddr_t>(rsdp->xsdt_address);
        m_is_xsdt = true;
    } else {
        m_sdt_root_phys = static_cast<paddr_t>(rsdp->rsdt_address);
        m_is_xsdt = false;
    }

    m_initialized = (m_sdt_root_phys != 0);
}

bool is_ready()
{
    return m_initialized;
}

sdt_view find_table(const char signature[4])
{
    if (!m_initialized)
        return {};

    const auto *root_header = static_cast<const wire::sdt_header *>(
        kernel::mm::vmm::phy_to_vrt(m_sdt_root_phys)
    );

    if (!root_header || !validate_checksum(root_header, root_header->length))
        return {};

    const usize header_size = sizeof(wire::sdt_header);
    if (root_header->length < header_size)
        return {};

    const usize entries_bytes = root_header->length - header_size;
    const auto *entries_base = reinterpret_cast<const u8 *>(root_header) + header_size;

    if (m_is_xsdt) {
        const usize entry_count = entries_bytes / sizeof(u64);
        const auto *ptrs = reinterpret_cast<const u64 *>(entries_base);

        for (usize i = 0; i < entry_count; ++i) {
            const auto table_phys = static_cast<paddr_t>(ptrs[i]);
            if (table_phys == 0)
                continue;

            const auto *header = static_cast<const wire::sdt_header *>(
                kernel::mm::vmm::phy_to_vrt(table_phys)
            );

            if (header && signature_match(header->signature, signature)) {
                if (validate_checksum(header, header->length))
                    return sdt_view{header};
            }
        }
    } else {
        const usize entry_count = entries_bytes / sizeof(u32);
        const auto *ptrs = reinterpret_cast<const u32 *>(entries_base);

        for (usize i = 0; i < entry_count; ++i) {
            const auto table_phys = static_cast<paddr_t>(ptrs[i]);
            if (table_phys == 0)
                continue;

            const auto *header = static_cast<const wire::sdt_header *>(
                kernel::mm::vmm::phy_to_vrt(table_phys)
            );

            if (header && signature_match(header->signature, signature)) {
                if (validate_checksum(header, header->length))
                    return sdt_view{header};
            }
        }
    }

    return {};
}

kernel::init::init_result component::init_acpi()
{
    if (!kernel::boot::boot_info::available())
        return kernel::core::Err(kernel::init::init_error::DEPENDENCY_UNAVAILABLE);

    const auto &boot_info = kernel::boot::boot_info::current();
    if (!boot_info.acpi.present || boot_info.acpi.rsdp == 0)
        return kernel::core::Err(kernel::init::init_error::INVALID_BOOT_DATA);

    kernel::acpi::init(boot_info.acpi.rsdp);

    if (!is_ready())
        return kernel::core::Err(kernel::init::init_error::INVALID_BOOT_DATA);

    return kernel::core::Ok();
}

}  // namespace kernel::acpi
