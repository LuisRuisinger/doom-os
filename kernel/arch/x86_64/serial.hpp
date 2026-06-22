#ifndef DOOM_OS_KERNEL_ARCH_X86_64_SERIAL_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_SERIAL_HPP_

#include "kernel/boot/component.hpp"

namespace kernel::arch::x86_64::serial {
// =================================================================================================
// Public API
// =================================================================================================

void init();

bool can_write();

void write_char(char c);

void write(const char *s);

// =================================================================================================
// Boot component
// =================================================================================================

struct component : kernel::boot::component_base<component> {
    static constexpr const char *name = "serial";

    static bool init_component() {
        init();
        return true;
    }
};
}  // namespace kernel::arch::x86_64::serial

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_SERIAL_HPP_