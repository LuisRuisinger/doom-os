#ifndef DOOM_OS_KERNEL_BOOT_MULTIBOOT2_HPP_
#define DOOM_OS_KERNEL_BOOT_MULTIBOOT2_HPP_

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
using kernel::core::uptr;
using kernel::core::usize;

// =================================================================================================
// Constants
// =================================================================================================

inline constexpr u32 BOOTLOADER_MAGIC = 0x36D76289;

// =================================================================================================
// Tag types
// =================================================================================================

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

// =================================================================================================
// Multiboot2 wire structs
// =================================================================================================

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

static_assert(sizeof(fixed_header) == 8);
static_assert(sizeof(tag_header) == 8);
static_assert(sizeof(string_tag) == 8);
static_assert(sizeof(basic_memory_tag) == 16);
static_assert(sizeof(module_tag) == 16);
static_assert(sizeof(memory_map_tag) == 16);
static_assert(sizeof(memory_map_entry) == 24);
static_assert(sizeof(framebuffer_tag) == 32);
static_assert(sizeof(acpi_rsdp_v1) == 20);

// =================================================================================================
// Access helpers
// =================================================================================================

constexpr usize align_tag_size(usize size)
{
    return (size + 7) & ~usize{7};
}

inline const tag_header *first_tag(const fixed_header &header)
{
    return ((&header) as(const u8 *) + sizeof(fixed_header)) as(const tag_header *);
}

inline const tag_header *next_tag(const tag_header &tag)
{
    return ((&tag) as(const u8 *) + align_tag_size(tag.size)) as(const tag_header *);
}

inline const char *string_payload(const string_tag &tag)
{
    return (&tag) as(const char *) + sizeof(string_tag);
}

inline const char *module_command_line(const module_tag &tag)
{
    return (&tag) as(const char *) + sizeof(module_tag);
}

inline usize memory_map_entry_count(const memory_map_tag &tag)
{
    if (tag.entry_size < sizeof(memory_map_entry) || tag.header.size < sizeof(memory_map_tag))
        return 0;

    return (tag.header.size - sizeof(memory_map_tag)) / tag.entry_size;
}

inline const memory_map_entry *memory_map_entry_at(const memory_map_tag &tag, usize index)
{
    return ((&tag) as(const u8 *) + sizeof(memory_map_tag) + index * tag.entry_size)
        as(const memory_map_entry *);
}

inline const acpi_rsdp_v1 *acpi_rsdp(const tag_header &tag)
{
    return ((&tag) as(const u8 *) + sizeof(tag_header)) as(const acpi_rsdp_v1 *);
}

}  // namespace kernel::boot::multiboot2

#endif  // DOOM_OS_KERNEL_BOOT_MULTIBOOT2_HPP_
