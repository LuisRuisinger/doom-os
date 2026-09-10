#ifndef DOOM_OS_KERNEL_ACPI_ACPI_HPP_
#define DOOM_OS_KERNEL_ACPI_ACPI_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/init/component.hpp"
#include "kernel/boot/boot_info.hpp"
#include "kernel/core/types.hpp"
#include "kernel/mm/vmm.hpp"

namespace kernel::acpi {

using kernel::core::paddr_t;
using kernel::core::u16;
using kernel::core::u32;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;
using kernel::core::vaddr_t;

namespace wire {

struct [[gnu::packed]] rsdp_descriptor {
    char signature[8];
    u8   checksum;
    char oem_id[6];
    u8   revision;
    u32  rsdt_address;
    u32  length;
    u64  xsdt_address;
    u8   extended_checksum;
    u8   reserved[3];
};

struct [[gnu::packed]] sdt_header {
    char signature[4];
    u32  length;
    u8   revision;
    u8   checksum;
    char oem_id[6];
    char oem_table_id[8];
    u32  oem_revision;
    u32  creator_id;
    u32  creator_revision;
};

}  // namespace wire

struct sdt_view {
    const wire::sdt_header *header{};

    [[nodiscard]] bool valid() const
    {
        return header != nullptr;
    }

    [[nodiscard]] const char *signature() const
    {
        return header ? header->signature : "";
    }

    [[nodiscard]] u32 length() const
    {
        return header ? header->length : 0;
    }

    [[nodiscard]] const void *body() const
    {
        if (!header)
            return nullptr;
        return reinterpret_cast<const u8 *>(header) + sizeof(wire::sdt_header);
    }

    [[nodiscard]] usize body_size() const
    {
        if (!header || header->length < sizeof(wire::sdt_header))
            return 0;
        return header->length - sizeof(wire::sdt_header);
    }
};

void init(paddr_t rsdp_address);
[[nodiscard]] bool is_ready();
[[nodiscard]] bool validate_checksum(const void *pointer, usize length);
[[nodiscard]] sdt_view find_table(const char signature[4]);

struct component : kernel::init::component<component, kernel::init::no_resource,
                                           kernel::boot::boot_info::component,
                                           kernel::mm::vmm::component> {
    static constexpr auto *name = "ACPI";

    static kernel::init::init_result init_acpi();

    template <typename View>
    static kernel::init::init_result init(View)
    {
        return init_acpi();
    }
};

}  // namespace kernel::acpi

#endif  // DOOM_OS_KERNEL_ACPI_ACPI_HPP_
