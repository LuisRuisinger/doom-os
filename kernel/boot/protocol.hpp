#ifndef DOOM_OS_KERNEL_BOOT_PROTOCOL_HPP_
#define DOOM_OS_KERNEL_BOOT_PROTOCOL_HPP_

// =================================================================================================
// Cpp stdlib files
// =================================================================================================

#include <concepts>

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/component.hpp"
#include "kernel/core/types.hpp"
#include "kernel/debug/formatter.hpp"

namespace kernel::boot {

using kernel::core::paddr_t;
using kernel::core::u32;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

// =================================================================================================
// Fixed tables
// =================================================================================================

template <typename Entry, usize Capacity>
struct fixed_table {
    static constexpr usize CAPACITY = Capacity;

    Entry entries[Capacity]{};
    usize count{};
    bool  truncated{};

    // Nodiscard so that dropping an entry is always a deliberate act. A caller that pushes a
    // whole run and checks `truncated` once at the end discards each result explicitly; a
    // caller that forgets to check either way gets a compile error.
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

    void clear()
    {
        count = 0;
        truncated = false;
    }
};

// =================================================================================================
// Owned string
//
// The boot description outlives the buffer the bootloader handed us, so strings are copied
// rather than pointed at. Always NUL-terminated; over-long input is truncated and flagged.
// =================================================================================================

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

// =================================================================================================
// Capacities
// =================================================================================================

inline constexpr usize MAX_BOOT_MEMORY_REGIONS = 128;
inline constexpr usize MAX_BOOT_MODULES = 32;
inline constexpr usize MAX_BOOT_RESERVED_RANGES = 8;
inline constexpr usize MAX_BOOT_MODULE_COMMAND_LINE = 128;

// =================================================================================================
// Neutral boot description
//
// Nothing below names a boot protocol. An adapter translates whatever the bootloader handed
// us into these types, so a second protocol is a new adapter rather than a change to every
// consumer.
// =================================================================================================

enum class memory_kind : u32 {
    USABLE,
    RESERVED,
    ACPI_RECLAIMABLE,
    ACPI_NVS,
    DEFECTIVE,
};

struct address_range {
    paddr_t base{};
    u64     length{};
};

struct memory_region {
    paddr_t     base{};
    u64         length{};
    memory_kind kind{};
};

struct module_info {
    address_range                                range{};
    bounded_string<MAX_BOOT_MODULE_COMMAND_LINE> command_line{};
};

struct framebuffer_info {
    bool    present{};
    paddr_t address{};
    u32     pitch{};
    u32     width{};
    u32     height{};
    u8      bits_per_pixel{};
};

struct acpi_info {
    bool    present{};
    paddr_t rsdp{};
    u8      revision{};
};

struct info {
    bool valid{};

    bounded_string<MAX_BOOT_STRING> command_line{};
    bounded_string<MAX_BOOT_STRING> bootloader_name{};

    fixed_table<memory_region, MAX_BOOT_MEMORY_REGIONS> memory_map{};
    fixed_table<module_info, MAX_BOOT_MODULES>          modules{};

    // Physical ranges the boot protocol occupies with its own structures. Consumers reserve
    // these without needing to know what put them there.
    fixed_table<address_range, MAX_BOOT_RESERVED_RANGES> reserved{};

    framebuffer_info framebuffer{};
    acpi_info        acpi{};

    // True if any table dropped an entry. A description that lost regions is not a
    // description of this machine, and silently allocating around the ones that survived
    // is worse than refusing to boot.
    [[nodiscard]] bool truncated() const
    {
        return memory_map.truncated || modules.truncated || reserved.truncated;
    }

    // Reset in place. An info is around ten kilobytes, so assigning a fresh one would put
    // that much on a sixteen-kilobyte kernel stack as a temporary.
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

// =================================================================================================
// Handoff
//
// What the entry point was given. Deliberately shapeless: a magic number and a physical
// address covers multiboot2, and a pointer-sized handoff covers the protocols likely to
// follow it.
// =================================================================================================

struct handoff {
    u64     magic{};
    paddr_t address{};
};

// =================================================================================================
// Boot protocol
//
// The strategy the boot_info component runs. Selected once, in the boot wiring header.
// =================================================================================================

template <typename T>
concept boot_protocol = requires(const handoff &source, info &out) {
    { T::name } -> std::convertible_to<const char *>;
    { T::parse(source, out) } -> std::same_as<kernel::core::init_result>;
};

}  // namespace kernel::boot

// =================================================================================================
// Formatting
//
// bounded_string keeps its storage private, so reflect cannot walk it and the aggregate formatter
// will not accept it. Without this leaf, printing anything that contains one - module_info, and
// so the module table - fails deep inside the field walker rather than here. The string is always
// NUL-terminated, so the ordinary string emitter is all it needs.
// =================================================================================================

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
