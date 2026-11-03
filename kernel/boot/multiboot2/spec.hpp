#ifndef DOOM_OS_KERNEL_BOOT_MULTIBOOT2_SPEC_HPP_
#define DOOM_OS_KERNEL_BOOT_MULTIBOOT2_SPEC_HPP_

// =================================================================================================
// Standard library
// =================================================================================================

#include <cstddef>
#include <span>
#include <string_view>

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/cast.hpp"
#include "kernel/core/types.hpp"

namespace kernel::boot::multiboot2 {

using kernel::core::u16;
using kernel::core::u32;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

inline constexpr u32   BOOTLOADER_MAGIC = 0x36D76289;
inline constexpr usize TAG_ALIGNMENT = 8;
inline constexpr u32   NO_PARTITION = 0xFFFFFFFF;

enum class tag_type : u32 {
    END = 0,
    COMMAND_LINE = 1,
    BOOTLOADER_NAME = 2,
    MODULE = 3,
    BASIC_MEMORY = 4,
    BIOS_BOOT_DEVICE = 5,
    MEMORY_MAP = 6,
    VBE = 7,
    FRAMEBUFFER = 8,
    ELF_SECTIONS = 9,
    APM = 10,
    EFI32_SYSTEM_TABLE = 11,
    EFI64_SYSTEM_TABLE = 12,
    SMBIOS = 13,
    ACPI_OLD = 14,
    ACPI_NEW = 15,
    NETWORK = 16,
    EFI_MEMORY_MAP = 17,
    EFI_BOOT_SERVICES = 18,
    EFI32_IMAGE_HANDLE = 19,
    EFI64_IMAGE_HANDLE = 20,
    LOAD_BASE_ADDRESS = 21,
};

enum class memory_type : u32 {
    AVAILABLE = 1,
    RESERVED = 2,
    ACPI_RECLAIMABLE = 3,
    ACPI_NVS = 4,
    BAD_MEMORY = 5,
};

enum class framebuffer_type : u8 {
    INDEXED = 0,
    RGB = 1,
    EGA_TEXT = 2,
};

struct fixed_header {
    u32 total_size;
    u32 reserved;
};

struct tag_header {
    u32 type;
    u32 size;
};

namespace detail {

template <typename Tag>
[[nodiscard]] std::span<const u8> payload(const Tag &tag)
{
    if (tag.header.size <= sizeof(Tag))
        return {};

    return {(&tag) as(const u8 *) + sizeof(Tag), tag.header.size - sizeof(Tag)};
}

[[nodiscard]] inline std::string_view bounded_string(std::span<const u8> bytes)
{
    const auto *text = bytes.data() as(const char *);

    usize length = 0;
    while (length < bytes.size() && text[length] != '\0')
        ++length;

    return {text, length};
}

}  // namespace detail

struct string_tag {
    tag_header header;

    [[nodiscard]] std::string_view value() const
    {
        return detail::bounded_string(detail::payload(*this));
    }
};

struct basic_memory_tag {
    tag_header header;
    u32        lower_memory_kib;
    u32        upper_memory_kib;
};

struct bios_boot_device_tag {
    tag_header header;
    u32        bios_device;
    u32        partition;
    u32        sub_partition;
};

struct module_tag {
    tag_header header;
    u32        mod_start;
    u32        mod_end;

    [[nodiscard]] std::string_view command_line() const
    {
        return detail::bounded_string(detail::payload(*this));
    }
};

struct elf_sections_tag {
    tag_header header;
    u32        count;
    u32        entry_size;
    u32        string_table_index;

    [[nodiscard]] std::span<const u8> sections() const
    {
        return detail::payload(*this);
    }
};

struct memory_map_tag {
    tag_header header;
    u32        entry_size;
    u32        entry_version;
};

struct memory_map_entry {
    u64 base_addr;
    u64 length;
    u32 type;
    u32 reserved;
};

struct apm_tag {
    tag_header header;
    u16        version;
    u16        code_segment;
    u32        offset;
    u16        code_segment_16;
    u16        data_segment;
    u16        flags;
    u16        code_segment_length;
    u16        code_segment_16_length;
    u16        data_segment_length;
};

struct vbe_tag {
    tag_header header;
    u16        mode;
    u16        interface_segment;
    u16        interface_offset;
    u16        interface_length;
    u8         control_info[512];
    u8         mode_info[256];
};

struct framebuffer_color {
    u8 red;
    u8 green;
    u8 blue;
};

struct framebuffer_rgb {
    u8 red_position;
    u8 red_mask_size;
    u8 green_position;
    u8 green_mask_size;
    u8 blue_position;
    u8 blue_mask_size;
};

struct framebuffer_tag {
    tag_header header;
    u64        address;
    u32        pitch;
    u32        width;
    u32        height;
    u8         bits_per_pixel;
    u8         type;
    u16        reserved;

    [[nodiscard]] std::span<const framebuffer_color> palette() const
    {
        const std::span<const u8> bytes = detail::payload(*this);

        if (type != framebuffer_type::INDEXED as(u8) || bytes.size() < sizeof(u16))
            return {};

        const usize count = *(bytes.data() as(const u16 *));
        const usize fits = (bytes.size() - sizeof(u16)) / sizeof(framebuffer_color);

        return {(bytes.data() + sizeof(u16)) as(const framebuffer_color *),
                count < fits ? count : fits};
    }

    [[nodiscard]] const framebuffer_rgb *rgb() const
    {
        const std::span<const u8> bytes = detail::payload(*this);

        if (type != framebuffer_type::RGB as(u8) || bytes.size() < sizeof(framebuffer_rgb))
            return nullptr;

        return bytes.data() as(const framebuffer_rgb *);
    }
};

struct efi32_pointer_tag {
    tag_header header;
    u32        pointer;
};

struct efi64_pointer_tag {
    tag_header header;
    u64        pointer;
};

struct smbios_tag {
    tag_header header;
    u8         major;
    u8         minor;
    u8         reserved[6];

    [[nodiscard]] std::span<const u8> tables() const
    {
        return detail::payload(*this);
    }
};

struct acpi_rsdp_v1 {
    char signature[8];
    u8   checksum;
    char oem_id[6];
    u8   revision;
    u32  rsdt_address;
};

struct acpi_tag {
    tag_header   header;
    acpi_rsdp_v1 rsdp;
};

struct network_tag {
    tag_header header;

    [[nodiscard]] std::span<const u8> dhcp_ack() const
    {
        return detail::payload(*this);
    }
};

struct efi_memory_map_tag {
    tag_header header;
    u32        descriptor_size;
    u32        descriptor_version;

    [[nodiscard]] std::span<const u8> descriptors() const
    {
        return detail::payload(*this);
    }
};

struct load_base_address_tag {
    tag_header header;
    u32        load_base_address;
};

static_assert(sizeof(fixed_header) == 8);
static_assert(sizeof(tag_header) == 8);
static_assert(sizeof(string_tag) == 8);
static_assert(sizeof(basic_memory_tag) == 16);
static_assert(sizeof(bios_boot_device_tag) == 20);
static_assert(sizeof(module_tag) == 16);
static_assert(sizeof(elf_sections_tag) == 20);
static_assert(sizeof(memory_map_tag) == 16);
static_assert(sizeof(memory_map_entry) == 24);
static_assert(sizeof(apm_tag) == 28);
static_assert(sizeof(vbe_tag) == 784);
static_assert(sizeof(framebuffer_color) == 3);
static_assert(sizeof(framebuffer_rgb) == 6);
static_assert(sizeof(framebuffer_tag) == 32);
static_assert(sizeof(efi32_pointer_tag) == 12);
static_assert(sizeof(efi64_pointer_tag) == 16);
static_assert(sizeof(smbios_tag) == 16);
static_assert(sizeof(acpi_rsdp_v1) == 20);
static_assert(sizeof(acpi_tag) == 28);
static_assert(sizeof(network_tag) == 8);
static_assert(sizeof(efi_memory_map_tag) == 16);
static_assert(sizeof(load_base_address_tag) == 12);

static_assert(offsetof(apm_tag, offset) == 12);
static_assert(offsetof(elf_sections_tag, string_table_index) == 16);
static_assert(offsetof(framebuffer_tag, type) == 29);

}  // namespace kernel::boot::multiboot2

#endif  // DOOM_OS_KERNEL_BOOT_MULTIBOOT2_SPEC_HPP_
