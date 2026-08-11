#include "kernel/boot/boot_info.hpp"

namespace kernel::boot::boot_info {

namespace {

using kernel::core::u8;
using kernel::core::uptr;
using kernel::core::usize;

multiboot2_handoff g_handoff{};
info               g_info{};

template <typename T>
[[nodiscard]] const T *as_tag(const multiboot2::tag_header &tag, const u8 *buffer_end)
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

[[nodiscard]] bool parse_memory_map(const multiboot2::memory_map_tag &tag, info &result)
{
    if (tag.entry_size < sizeof(multiboot2::memory_map_entry))
        return false;

    result.raw_memory_map = &tag;
    const usize count = multiboot2::memory_map_entry_count(tag);

    for (usize i = 0; i < count; ++i) {
        const auto *entry = multiboot2::memory_map_entry_at(tag, i);

        result.memory_map.push(memory_region{
            .base_address = entry->base_addr,
            .length = entry->length,
            .type = static_cast<multiboot2::memory_type>(entry->type),
            .reserved = entry->reserved,
        });
    }

    return true;
}

[[nodiscard]] bool parse_tag(const multiboot2::tag_header &tag, const u8 *buffer_end, info &result)
{
    using enum multiboot2::tag_type;

    switch (static_cast<multiboot2::tag_type>(tag.type)) {
        case COMMAND_LINE: {
            const auto *value = as_tag<multiboot2::string_tag>(tag, buffer_end);

            if (!value)
                return false;

            result.command_line = value;
            return true;
        }

        case BOOTLOADER_NAME: {
            const auto *value = as_tag<multiboot2::string_tag>(tag, buffer_end);

            if (!value)
                return false;

            result.bootloader_name = value;
            return true;
        }

        case MODULE: {
            const auto *value = as_tag<multiboot2::module_tag>(tag, buffer_end);

            if (!value)
                return false;

            result.modules.push(value);
            return true;
        }

        case BASIC_MEMORY: {
            const auto *value = as_tag<multiboot2::basic_memory_tag>(tag, buffer_end);

            if (!value)
                return false;

            result.basic_memory = value;
            return true;
        }

        case MEMORY_MAP: {
            const auto *value = as_tag<multiboot2::memory_map_tag>(tag, buffer_end);

            return value && parse_memory_map(*value, result);
        }

        case FRAMEBUFFER: {
            const auto *value = as_tag<multiboot2::framebuffer_tag>(tag, buffer_end);

            if (!value)
                return false;

            result.framebuffer = value;
            return true;
        }

        case ACPI_OLD:
            result.acpi_old = &tag;
            return true;

        case ACPI_NEW:
            result.acpi_new = &tag;
            return true;

        default:
            return true;
    }
}

[[nodiscard]] bool parse_multiboot2(multiboot2_handoff handoff, info &result)
{
    if (handoff.magic != multiboot2::BOOTLOADER_MAGIC)
        return false;

    if (handoff.info_address == 0)
        return false;

    const auto *header =
        reinterpret_cast<const multiboot2::fixed_header *>(static_cast<uptr>(handoff.info_address));

    if (header->total_size < sizeof(*header))
        return false;

    const auto *begin = reinterpret_cast<const u8 *>(header);

    if (header->total_size > static_cast<usize>(~uptr{0}) - reinterpret_cast<uptr>(begin)) {
        return false;
    }

    const auto *end = begin + header->total_size;
    const auto *tag = multiboot2::first_tag(*header);

    result.header = header;

    while (reinterpret_cast<const u8 *>(tag) < end) {
        const auto *tag_begin = reinterpret_cast<const u8 *>(tag);

        if (static_cast<usize>(end - tag_begin) < sizeof(*tag))
            return false;

        if (tag->size < sizeof(*tag))
            return false;

        if (tag->size > static_cast<usize>(end - tag_begin))
            return false;

        if (static_cast<multiboot2::tag_type>(tag->type) == multiboot2::tag_type::END) {
            return tag->size == sizeof(multiboot2::tag_header);
        }

        if (!parse_tag(*tag, end, result))
            return false;

        const auto *next = multiboot2::next_tag(*tag);

        if (reinterpret_cast<const u8 *>(next) <= tag_begin)
            return false;

        tag = next;
    }

    return false;
}

[[nodiscard]] info make_empty_info(const multiboot2_handoff handoff)
{
    return info{.handoff = handoff};
}

}  // namespace

void set_handoff(multiboot2_handoff handoff)
{
    g_handoff = handoff;
    g_info = make_empty_info(handoff);
}

void set_handoff(u64 magic, paddr_t info_address)
{
    set_handoff({
        .magic = magic,
        .info_address = info_address,
    });
}

const multiboot2_handoff &handoff()
{
    return g_handoff;
}

const info &current()
{
    return g_info;
}

bool available()
{
    return g_info.valid;
}

kernel::boot::init_result component::parse_handoff()
{
    auto parsed = make_empty_info(g_handoff);

    if (!parse_multiboot2(g_handoff, parsed)) {
        g_info = make_empty_info(g_handoff);
        return kernel::boot::Err(kernel::boot::init_error::INVALID_BOOT_DATA);
    }

    parsed.valid = true;
    g_info = parsed;
    return kernel::boot::Ok();
}

}  // namespace kernel::boot::boot_info