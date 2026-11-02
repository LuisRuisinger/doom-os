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
        out.memory_map.emplace_back(entry.base_addr, entry.length,
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

void translate_framebuffer(const framebuffer_tag &tag, kernel::boot::info &out)
{
    const kernel::boot::address_range range(tag.address, tag.pitch as(u64) * tag.height);
    out.framebuffer = kernel::boot::framebuffer_info(true, range, tag.pitch, tag.width, tag.height,
                                                     tag.bits_per_pixel);
}

[[nodiscard]] bool translate_acpi(const acpi_tag &tag, u8 revision, kernel::boot::info &out)
{
    if (out.acpi.present && out.acpi.revision >= revision)
        return true;

    const kernel::boot::address_range rsdp((&tag.rsdp) as(paddr_t),
                                           tag.header.size - sizeof(tag_header));
    out.acpi = kernel::boot::acpi_info(true, rsdp, revision);

    return true;
}

[[nodiscard]] bool translate_tag(const tag_header &tag, kernel::boot::info &out)
{
    using enum tag_type;

    switch (tag.type as(tag_type)) {
        case COMMAND_LINE: {
            const auto *value = as_tag<string_tag>(tag);

            if (value == nullptr)
                return false;

            const std::string_view text = value->value();

            out.command_line.assign(text.data(), text.size());
            return true;
        }

        case BOOTLOADER_NAME: {
            const auto *value = as_tag<string_tag>(tag);

            if (value == nullptr)
                return false;

            const std::string_view text = value->value();

            out.bootloader_name.assign(text.data(), text.size());
            return true;
        }

        case MODULE: {
            const auto *value = as_tag<module_tag>(tag);

            return value != nullptr && translate_module(*value, out);
        }

        case MEMORY_MAP: {
            const auto *value = as_tag<memory_map_tag>(tag);

            return value != nullptr && translate_memory_map(*value, out);
        }

        case FRAMEBUFFER: {
            const auto *value = as_tag<framebuffer_tag>(tag);

            if (value == nullptr)
                return false;

            translate_framebuffer(*value, out);
            return true;
        }

        case ACPI_OLD: {
            const auto *value = as_tag<acpi_tag>(tag);

            return value != nullptr && translate_acpi(*value, 1, out);
        }

        case ACPI_NEW: {
            const auto *value = as_tag<acpi_tag>(tag);

            return value != nullptr && translate_acpi(*value, 2, out);
        }

        case BASIC_MEMORY:
        case END:
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
