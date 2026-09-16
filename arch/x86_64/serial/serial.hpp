#ifndef DOOM_OS_KERNEL_ARCH_X86_64_SERIAL_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_SERIAL_HPP_

#include "kernel/init/component.hpp"

namespace kernel::arch::x86_64::serial {

void init();

bool can_write();

void write_char(char c);

void write(const char *s);

void attach_console();

struct component : kernel::init::component<component, kernel::init::no_resource> {
    static constexpr const char *name = "SERIAL";

    template <typename View>
    static kernel::init::init_result init(View)
    {
        kernel::arch::x86_64::serial::init();
        kernel::arch::x86_64::serial::attach_console();

        return kernel::core::Ok();
    }
};

}  // namespace kernel::arch::x86_64::serial

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_SERIAL_HPP_
