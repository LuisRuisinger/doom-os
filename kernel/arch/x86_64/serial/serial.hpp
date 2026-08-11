#ifndef DOOM_OS_KERNEL_ARCH_X86_64_SERIAL_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_SERIAL_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

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

struct component : kernel::boot::component<component, kernel::boot::no_resource> {
    static constexpr const char *name = "SERIAL";

    template <typename View>
    static kernel::boot::init_result init(View)
    {
        kernel::arch::x86_64::serial::init();
        return kernel::boot::Ok();
    }
};
}  // namespace kernel::arch::x86_64::serial

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_SERIAL_HPP_