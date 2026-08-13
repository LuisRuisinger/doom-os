// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/multiboot2_adapter.hpp"

namespace kernel::boot::multiboot2 {

namespace {

using kernel::core::u8;
using kernel::core::uptr;
using kernel::core::usize;

// =================================================================================================
// Bounds-checked tag access
// =================================================================================================

template <typename T>
[[nodiscard]] const T *as_tag(const tag_header &tag, const u8 *buffer_end)
{
    const auto *begin = reinterpret_cast<const u8 *>(&tag);

    if (tag.size < sizeof(T))
        return nullptr;

    if (begin > buffer_end)
        return nullptr;

    const auto remaining = static_cast<usize>(buffer_end - begin);
    if (tag.size > remaining)
        return nullptr;

    return reinterpret_cast<const T *>(&tag);
}

// =================================================================================================
// Translation
// =================================================================================================

[[nodiscard]] kernel::boot::memory_kind translate_memory_type(kernel::core::u32 type)
{
    switch (static_cast<memory_type>(type)) {
        case memory_type::AVAILABLE:
            return kernel::boot::memory_kind::USABLE;
        case memory_type::ACPI_RECLAIMABLE:
            return kernel::boot::memory_kind::ACPI_RECLAIMABLE;
        case memory_type::ACPI_NVS:
            return kernel::boot::memory_kind::ACPI_NVS;
        case memory_type::BAD_MEMORY:
            return kernel::boot::memory_kind::DEFECTIVE;
        case memory_type::RESERVED:
            break;
    }

    // Anything a future revision adds is treated as untouchable rather than usable.
    return kernel::boot::memory_kind::RESERVED;
}

[[nodiscard]] bool translate_memory_map(const memory_map_tag &tag, kernel::boot::info &out)
{
    if (tag.entry_size < sizeof(memory_map_entry))
        return false;

    const usize count = memory_map_entry_count(tag);

    for (usize i = 0; i < count; ++i) {
        const auto *entry = memory_map_entry_at(tag, i);

        // Overflow is not handled per entry: the table records that it dropped one, and
        // boot_info rejects the whole description afterwards. Stopping here instead would
        // leave a map that looks complete.
        static_cast<void>(out.memory_map.push(kernel::boot::memory_region{
            .base = entry->base_addr,
            .length = entry->length,
            .kind = translate_memory_type(entry->type),
        }));
    }

    return true;
}

[[nodiscard]] bool translate_module(const module_tag &tag, kernel::boot::info &out)
{
    if (tag.mod_end <= tag.mod_start)
        return true;

    kernel::boot::module_info entry{};

    entry.range.base = tag.mod_start;
    entry.range.length = tag.mod_end - tag.mod_start;
    entry.command_line.assign(module_command_line(tag));

    static_cast<void>(out.modules.push(entry));
    return true;
}

void translate_framebuffer(const framebuffer_tag &tag, kernel::boot::info &out)
{
    out.framebuffer = kernel::boot::framebuffer_info{
        .present = true,
        .address = tag.address,
        .pitch = tag.pitch,
        .width = tag.width,
        .height = tag.height,
        .bits_per_pixel = tag.bits_per_pixel,
    };
}

void translate_acpi(const tag_header &tag, kernel::core::u8 revision, kernel::boot::info &out)
{
    const auto *rsdp = acpi_rsdp(tag);

    if (rsdp == nullptr)
        return;

    // A newer revision already recorded wins: the XSDT supersedes the RSDT.
    if (out.acpi.present && out.acpi.revision >= revision)
        return;

    out.acpi = kernel::boot::acpi_info{
        .present = true,
        .rsdp = static_cast<kernel::core::paddr_t>(reinterpret_cast<uptr>(rsdp)),
        .revision = revision,
    };
}

[[nodiscard]] bool translate_tag(const tag_header &tag, const u8 *buffer_end,
                                 kernel::boot::info &out)
{
    using enum tag_type;

    switch (static_cast<tag_type>(tag.type)) {
        case COMMAND_LINE: {
            const auto *value = as_tag<string_tag>(tag, buffer_end);

            if (!value)
                return false;

            out.command_line.assign(string_payload(*value));
            return true;
        }

        case BOOTLOADER_NAME: {
            const auto *value = as_tag<string_tag>(tag, buffer_end);

            if (!value)
                return false;

            out.bootloader_name.assign(string_payload(*value));
            return true;
        }

        case MODULE: {
            const auto *value = as_tag<module_tag>(tag, buffer_end);

            return value && translate_module(*value, out);
        }

        case MEMORY_MAP: {
            const auto *value = as_tag<memory_map_tag>(tag, buffer_end);

            return value && translate_memory_map(*value, out);
        }

        case FRAMEBUFFER: {
            const auto *value = as_tag<framebuffer_tag>(tag, buffer_end);

            if (!value)
                return false;

            translate_framebuffer(*value, out);
            return true;
        }

        case ACPI_OLD:
            translate_acpi(tag, 1, out);
            return true;

        case ACPI_NEW:
            translate_acpi(tag, 2, out);
            return true;

        case BASIC_MEMORY:
        case END:
        default:
            return true;
    }
}

}  // namespace

// =================================================================================================
// Multiboot2 adapter
// =================================================================================================

kernel::core::init_result adapter::parse(const kernel::boot::handoff &source,
                                         kernel::boot::info          &out)
{
    using kernel::core::init_error;

    if (source.magic != BOOTLOADER_MAGIC)
        return kernel::core::Err(init_error::INVALID_BOOT_DATA);

    if (source.address == 0)
        return kernel::core::Err(init_error::INVALID_BOOT_DATA);

    const auto *header = reinterpret_cast<const fixed_header *>(static_cast<uptr>(source.address));

    if (header->total_size < sizeof(*header))
        return kernel::core::Err(init_error::INVALID_BOOT_DATA);

    const auto *begin = reinterpret_cast<const u8 *>(header);

    if (header->total_size > static_cast<usize>(~uptr{0}) - reinterpret_cast<uptr>(begin))
        return kernel::core::Err(init_error::INVALID_BOOT_DATA);

    // The buffer we are about to read from is physical memory the bootloader owns. Record it
    // so it is never handed out, even though nothing points into it once parsing is done.
    static_cast<void>(out.reserved.push(kernel::boot::address_range{
        .base = source.address,
        .length = header->total_size,
    }));

    const auto *end = begin + header->total_size;
    const auto *tag = first_tag(*header);

    while (reinterpret_cast<const u8 *>(tag) < end) {
        const auto *tag_begin = reinterpret_cast<const u8 *>(tag);

        if (static_cast<usize>(end - tag_begin) < sizeof(*tag))
            return kernel::core::Err(init_error::INVALID_BOOT_DATA);

        if (tag->size < sizeof(*tag))
            return kernel::core::Err(init_error::INVALID_BOOT_DATA);

        if (tag->size > static_cast<usize>(end - tag_begin))
            return kernel::core::Err(init_error::INVALID_BOOT_DATA);

        if (static_cast<tag_type>(tag->type) == tag_type::END) {
            if (tag->size != sizeof(tag_header))
                return kernel::core::Err(init_error::INVALID_BOOT_DATA);

            return kernel::core::Ok();
        }

        if (!translate_tag(*tag, end, out))
            return kernel::core::Err(init_error::INVALID_BOOT_DATA);

        const auto *next = next_tag(*tag);

        if (reinterpret_cast<const u8 *>(next) <= tag_begin)
            return kernel::core::Err(init_error::INVALID_BOOT_DATA);

        tag = next;
    }

    // Ran off the end without an END tag.
    return kernel::core::Err(init_error::INVALID_BOOT_DATA);
}

}  // namespace kernel::boot::multiboot2
