#ifndef DOOM_OS_KERNEL_BOOT_PROTOCOL_HPP_
#define DOOM_OS_KERNEL_BOOT_PROTOCOL_HPP_

#include <concepts>

#include "kernel/core/types.hpp"
#include "kernel/debug/formatter.hpp"
#include "kernel/init/component.hpp"

namespace kernel::boot {

using kernel::core::paddr_t;
using kernel::core::u32;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

template <typename Entry, usize Capacity>
struct fixed_table {
    static constexpr usize CAPACITY = Capacity;

    Entry entries[Capacity]{};
    usize count{};
    bool  truncated{};

    [[nodiscard]] bool push(const Entry &entry)
    {
        if (count >= Capacity) {
            truncated = true;
            return false;
        }

        entries[count] = entry;
        ++count;
        return true;
    }

    Entry &operator[](usize index)
    {
        return entries[index];
    }

    const Entry &operator[](usize index) const
    {
        return entries[index];
    }

    [[nodiscard]] const Entry *begin() const
    {
        return entries;
    }

    [[nodiscard]] const Entry *end() const
    {
        return entries + count;
    }

    void clear()
    {
        count = 0;
        truncated = false;
    }
};

inline constexpr usize MAX_BOOT_STRING = 256;

template <usize Capacity>
class bounded_string {
    char  storage_m[Capacity]{};
    usize length_m{};
    bool  truncated_m{};

public:
    void assign(const char *source)
    {
        length_m = 0;
        truncated_m = false;

        if (source == nullptr) {
            storage_m[0] = '\0';
            return;
        }

        while (source[length_m] != '\0' && length_m + 1 < Capacity) {
            storage_m[length_m] = source[length_m];
            ++length_m;
        }

        storage_m[length_m] = '\0';
        truncated_m = source[length_m] != '\0';
    }

    [[nodiscard]] const char *c_str() const
    {
        return storage_m;
    }

    [[nodiscard]] usize length() const
    {
        return length_m;
    }

    [[nodiscard]] bool empty() const
    {
        return length_m == 0;
    }

    [[nodiscard]] bool truncated() const
    {
        return truncated_m;
    }
};

inline constexpr usize MAX_BOOT_MEMORY_REGIONS = 128;
inline constexpr usize MAX_BOOT_MODULES = 32;
inline constexpr usize MAX_BOOT_RESERVED_RANGES = 8;
inline constexpr usize MAX_BOOT_MODULE_COMMAND_LINE = 128;

enum class memory_kind : u32 {
    USABLE,
    RESERVED,
    ACPI_RECLAIMABLE,
    ACPI_NVS,
    DEFECTIVE,
};

struct address_range {
    paddr_t base;
    u64     length;
};

struct memory_region {
    paddr_t     base;
    u64         length;
    memory_kind kind;
};

struct module_info {
    address_range                                range;
    bounded_string<MAX_BOOT_MODULE_COMMAND_LINE> command_line;
};

struct framebuffer_info {
    bool          present;
    address_range range;
    u32           pitch;
    u32           width;
    u32           height;
    u8            bits_per_pixel;
};

struct acpi_info {
    bool          present;
    address_range rsdp;
    u8            revision;
};

struct info {
    bool valid;

    bounded_string<MAX_BOOT_STRING> command_line;
    bounded_string<MAX_BOOT_STRING> bootloader_name;

    fixed_table<memory_region, MAX_BOOT_MEMORY_REGIONS> memory_map;
    fixed_table<module_info, MAX_BOOT_MODULES>          modules;

    fixed_table<address_range, MAX_BOOT_RESERVED_RANGES> reserved;

    framebuffer_info framebuffer;
    acpi_info        acpi;

    [[nodiscard]] bool truncated() const
    {
        return memory_map.truncated || modules.truncated || reserved.truncated;
    }

    void reset()
    {
        valid = false;

        command_line.assign(nullptr);
        bootloader_name.assign(nullptr);

        memory_map.clear();
        modules.clear();
        reserved.clear();

        framebuffer = framebuffer_info{};
        acpi = acpi_info{};
    }
};

struct handoff {
    u64     magic;
    paddr_t address;
};

template <typename T>
concept boot_protocol = requires(const handoff &source, info &out) {
    { T::name } -> std::convertible_to<const char *>;
    { T::parse(source, out) } -> std::same_as<kernel::init::init_result>;
};

}  // namespace kernel::boot

namespace kernel::debug::detail {

template <kernel::core::usize Capacity>
struct formatter<kernel::boot::bounded_string<Capacity>> {
    template <format_spec Spec>
    static void emit(const kernel::boot::bounded_string<Capacity> &value)
    {
        emit_string_value<Spec>(value.c_str());
    }
};

}  // namespace kernel::debug::detail

#endif  // DOOM_OS_KERNEL_BOOT_PROTOCOL_HPP_
