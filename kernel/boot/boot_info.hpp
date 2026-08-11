#ifndef DOOM_OS_KERNEL_BOOT_BOOT_INFO_HPP_
#define DOOM_OS_KERNEL_BOOT_BOOT_INFO_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/component.hpp"
#include "kernel/boot/multiboot2.hpp"
#include "kernel/core/types.hpp"

namespace kernel::boot::boot_info {

using kernel::core::paddr_t;
using kernel::core::u32;
using kernel::core::u64;
using kernel::core::usize;

// =================================================================================================
// Fixed boot tables
// =================================================================================================

template <typename Entry, usize Capacity>
struct fixed_table {
    static constexpr usize CAPACITY = Capacity;

    Entry entries[Capacity]{};
    usize count{};
    bool  truncated{};

    bool push(const Entry &entry)
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
};

// =================================================================================================
// Static boot info
// =================================================================================================

inline constexpr usize MAX_BOOT_MEMORY_REGIONS = 128;
inline constexpr usize MAX_BOOT_MODULES = 32;

struct multiboot2_handoff {
    u64     magic{};
    paddr_t info_address{};
};

struct memory_region {
    paddr_t                 base_address{};
    u64                     length{};
    multiboot2::memory_type type{};
    u32                     reserved{};
};

struct info {
    bool valid{};

    multiboot2_handoff handoff{};

    const multiboot2::fixed_header     *header{};
    const multiboot2::string_tag       *command_line{};
    const multiboot2::string_tag       *bootloader_name{};
    const multiboot2::basic_memory_tag *basic_memory{};
    const multiboot2::memory_map_tag   *raw_memory_map{};
    const multiboot2::framebuffer_tag  *framebuffer{};
    const multiboot2::tag_header       *acpi_old{};
    const multiboot2::tag_header       *acpi_new{};

    fixed_table<memory_region, MAX_BOOT_MEMORY_REGIONS>           memory_map{};
    fixed_table<const multiboot2::module_tag *, MAX_BOOT_MODULES> modules{};
};

void set_handoff(multiboot2_handoff source);
void set_handoff(u64 magic, paddr_t info_address);
const multiboot2_handoff &handoff();
const info &current();
bool available();

// =================================================================================================
// Component
// =================================================================================================

struct component : kernel::boot::component<component, kernel::boot::no_resource> {
    static constexpr auto *name = "BOOT_INFO";

    static init_result parse_handoff();

    template <typename View>
    static init_result init(View)
    {
        return parse_handoff();
    }
};

}  // namespace kernel::boot::boot_info

#endif  // DOOM_OS_KERNEL_BOOT_BOOT_INFO_HPP_
