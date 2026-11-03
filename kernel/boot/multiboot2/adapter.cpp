#include "kernel/boot/multiboot2/adapter.hpp"

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/multiboot2/iterator.hpp"
#include "kernel/core/cast.hpp"

namespace kernel::boot::multiboot2 {

using kernel::core::paddr_t;
using kernel::core::u32;
using kernel::core::u8;

u64     m_magic{};
paddr_t m_info_address{};

template <typename T>
[[nodiscard]] const T *as_tag(const tag_header &tag)
{
    if (tag.size < sizeof(T))
        return nullptr;

    return (&tag) as(const T *);
}

template <typename T>
[[nodiscard]] bool translate_as(
    const tag_header &tag, kernel::boot::info &out,
    bool (*translate)(const T &, kernel::boot::info &))
{
    const T *value = as_tag<T>(tag);

    return value != nullptr && translate(*value, out);
}

[[nodiscard]] bool translate_command_line(const string_tag &tag, kernel::boot::info &out)
{
    const std::string_view text = tag.value();

    out.command_line.assign(text.data(), text.size());
    return true;
}

[[nodiscard]] bool translate_bootloader_name(const string_tag &tag, kernel::boot::info &out)
{
    const std::string_view text = tag.value();

    out.bootloader_name.assign(text.data(), text.size());
    return true;
}

[[nodiscard]] kernel::boot::memory_kind translate_memory_type(u32 type)
{
    switch (type as(memory_type)) {
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

    // Placeholder for any future tag to be handled
    return kernel::boot::memory_kind::RESERVED;
}

[[nodiscard]] bool translate_memory_map(const memory_map_tag &tag, kernel::boot::info &out)
{
    if (tag.entry_size < sizeof(memory_map_entry))
        return false;

    for (const memory_map_entry &entry : entries(tag))
        out.memory_map.emplace_back(
            entry.base_addr,
            entry.length,
            translate_memory_type(entry.type));

    return true;
}

[[nodiscard]] bool translate_module(const module_tag &tag, kernel::boot::info &out)
{
    if (tag.mod_end <= tag.mod_start)
        return true;

    auto &entry = out.modules.emplace_back(
        kernel::boot::address_range(tag.mod_start, tag.mod_end - tag.mod_start));

    const std::string_view command_line = tag.command_line();

    entry.command_line.assign(command_line.data(), command_line.size());
    return true;
}

[[nodiscard]] bool translate_framebuffer(const framebuffer_tag &tag, kernel::boot::info &out)
{
    using kernel::boot::framebuffer_kind;

    kernel::boot::framebuffer_info &framebuffer = out.framebuffer;
    framebuffer = kernel::boot::framebuffer_info{};

    switch (tag.type as(framebuffer_type)) {
        case framebuffer_type::RGB: {
            const framebuffer_rgb *rgb = tag.rgb();

            if (rgb == nullptr)
                return false;

            framebuffer.kind = framebuffer_kind::RGB;
            framebuffer.red = {.position = rgb->red_position, .size = rgb->red_mask_size};
            framebuffer.green = {.position = rgb->green_position, .size = rgb->green_mask_size};
            framebuffer.blue = {.position = rgb->blue_position, .size = rgb->blue_mask_size};
            break;
        }

        case framebuffer_type::INDEXED: {
            const std::span<const framebuffer_color> colors = tag.palette();

            if (colors.size() > kernel::boot::MAX_BOOT_PALETTE_COLORS)
                return false;

            framebuffer.kind = framebuffer_kind::INDEXED;
            for (const framebuffer_color &color : colors)
                framebuffer.palette.emplace_back(color.red, color.green, color.blue);

            break;
        }

        case framebuffer_type::EGA_TEXT:
            framebuffer.kind = framebuffer_kind::TEXT;
            break;

        default:
            return true;
    }

    framebuffer.present = true;
    framebuffer.range = kernel::boot::address_range(tag.address, tag.pitch as(u64) * tag.height);
    framebuffer.pitch = tag.pitch;
    framebuffer.width = tag.width;
    framebuffer.height = tag.height;
    framebuffer.bits_per_pixel = tag.bits_per_pixel;
    return true;
}

template <u8 Revision>
[[nodiscard]] bool translate_acpi(const acpi_tag &tag, kernel::boot::info &out)
{
    if (out.acpi.present && out.acpi.revision >= Revision)
        return true;

    const kernel::boot::address_range rsdp(
        (&tag.rsdp) as(paddr_t),
        tag.header.size - sizeof(tag_header));
    out.acpi = kernel::boot::acpi_info(true, rsdp, Revision);

    return true;
}

[[nodiscard]] bool translate_tag(const tag_header &tag, kernel::boot::info &out)
{
    using enum tag_type;

    switch (tag.type as(tag_type)) {
        case COMMAND_LINE:
            return translate_as(tag, out, translate_command_line);
        case BOOTLOADER_NAME:
            return translate_as(tag, out, translate_bootloader_name);
        case MODULE:
            return translate_as(tag, out, translate_module);
        case MEMORY_MAP:
            return translate_as(tag, out, translate_memory_map);
        case FRAMEBUFFER:
            return translate_as(tag, out, translate_framebuffer);
        case ACPI_OLD:
            return translate_as(tag, out, translate_acpi<1>);
        case ACPI_NEW:
            return translate_as(tag, out, translate_acpi<2>);
        default:
            return true;
    }
}

void set_handoff(u64 magic, paddr_t address)
{
    m_magic = magic;
    m_info_address = address;
}

bool adapter::well_formed()
{
    if (m_magic != BOOTLOADER_MAGIC)
        return false;

    if (m_info_address == 0)
        return false;

    return multiboot2::well_formed(*(m_info_address as(const fixed_header *)));
}

kernel::init::init_result adapter::parse(kernel::boot::info &out)
{
    using kernel::init::init_error;

    const auto *header = m_info_address as(const fixed_header *);

    out.reserved.emplace_back(m_info_address, header->total_size);

    for (const tag_header &tag : tags(*header)) {
        if (!translate_tag(tag, out))
            return kernel::core::Err(init_error::INVALID_BOOT_DATA);
    }

    return kernel::core::Ok();
}

}  // namespace kernel::boot::multiboot2

namespace kernel::boot::active {

bool well_formed()
{
    return multiboot2::adapter::well_formed();
}

kernel::init::init_result parse(info &out)
{
    return multiboot2::adapter::parse(out);
}

}  // namespace kernel::boot::active
