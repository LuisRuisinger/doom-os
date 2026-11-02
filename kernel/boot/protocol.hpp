#ifndef DOOM_OS_KERNEL_BOOT_PROTOCOL_HPP_
#define DOOM_OS_KERNEL_BOOT_PROTOCOL_HPP_

#include <concepts>

#include "kernel/core/inplace_vector.hpp"
#include "kernel/core/string.hpp"
#include "kernel/core/types.hpp"
#include "kernel/init/component.hpp"

namespace kernel::boot {

using kernel::core::paddr_t;
using kernel::core::u32;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;
using kernel::core::utils::fixed_string;
using kernel::core::utils::inplace_vector;

inline constexpr usize MAX_BOOT_STRING = 256;

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
    address_range                              range;
    fixed_string<MAX_BOOT_MODULE_COMMAND_LINE> command_line;
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
    fixed_string<MAX_BOOT_STRING> command_line;
    fixed_string<MAX_BOOT_STRING> bootloader_name;

    inplace_vector<memory_region, MAX_BOOT_MEMORY_REGIONS> memory_map;
    inplace_vector<module_info, MAX_BOOT_MODULES>          modules;

    inplace_vector<address_range, MAX_BOOT_RESERVED_RANGES> reserved;

    framebuffer_info framebuffer;
    acpi_info        acpi;

    void reset()
    {
        command_line.assign(nullptr);
        bootloader_name.assign(nullptr);

        memory_map.clear();
        modules.clear();
        reserved.clear();

        framebuffer = framebuffer_info{};
        acpi = acpi_info{};
    }
};

template <typename T>
concept boot_protocol = requires(info &out) {
    { T::name } -> std::convertible_to<const char *>;
    { T::parse(out) } -> std::same_as<kernel::init::init_result>;
};

}  // namespace kernel::boot

#endif  // DOOM_OS_KERNEL_BOOT_PROTOCOL_HPP_
