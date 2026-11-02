#ifndef DOOM_OS_KERNEL_BOOT_MULTIBOOT2_SPEC_HPP_
#define DOOM_OS_KERNEL_BOOT_MULTIBOOT2_SPEC_HPP_

// =================================================================================================
// Standard library
// =================================================================================================

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

enum class tag_type : u32 {
    END = 0,
    COMMAND_LINE = 1,
    BOOTLOADER_NAME = 2,
    MODULE = 3,
    BASIC_MEMORY = 4,
    MEMORY_MAP = 6,
    FRAMEBUFFER = 8,
    ACPI_OLD = 14,
    ACPI_NEW = 15,
};

enum class memory_type : u32 {
    AVAILABLE = 1,
    RESERVED = 2,
    ACPI_RECLAIMABLE = 3,
    ACPI_NVS = 4,
    BAD_MEMORY = 5,
};

struct fixed_header {
    u32 total_size;
    u32 reserved;
};

struct tag_header {
    u32 type;
    u32 size;
};

struct string_tag {
    tag_header header;

    [[nodiscard]] std::string_view value() const
    {
        const auto *text = (this as(const u8 *) + sizeof(string_tag)) as(const char *);
        const usize capacity = header.size - sizeof(string_tag);

        usize length = 0;
        while (length < capacity && text[length] != '\0')
            ++length;

        return {text, length};
    }
};

struct basic_memory_tag {
    tag_header header;
    u32        lower_memory_kib;
    u32        upper_memory_kib;
};

struct module_tag {
    tag_header header;
    u32        mod_start;
    u32        mod_end;

    [[nodiscard]] std::string_view command_line() const
    {
        const auto *text = (this as(const u8 *) + sizeof(module_tag)) as(const char *);
        const usize capacity = header.size - sizeof(module_tag);

        usize length = 0;
        while (length < capacity && text[length] != '\0')
            ++length;

        return {text, length};
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

struct framebuffer_tag {
    tag_header header;
    u64        address;
    u32        pitch;
    u32        width;
    u32        height;
    u8         bits_per_pixel;
    u8         framebuffer_type;
    u16        reserved;
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

static_assert(sizeof(fixed_header) == 8);
static_assert(sizeof(tag_header) == 8);
static_assert(sizeof(string_tag) == 8);
static_assert(sizeof(basic_memory_tag) == 16);
static_assert(sizeof(module_tag) == 16);
static_assert(sizeof(memory_map_tag) == 16);
static_assert(sizeof(memory_map_entry) == 24);
static_assert(sizeof(framebuffer_tag) == 32);
static_assert(sizeof(acpi_rsdp_v1) == 20);
static_assert(sizeof(acpi_tag) == 28);

}  // namespace kernel::boot::multiboot2

#endif  // DOOM_OS_KERNEL_BOOT_MULTIBOOT2_SPEC_HPP_
