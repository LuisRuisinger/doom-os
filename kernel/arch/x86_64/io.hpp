#ifndef DOOM_OS_KERNEL_ARCH_X86_64_IO_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_IO_HPP_

// =================================================================================================
// Cpp stdlib files
// =================================================================================================

#include <stdint.h>

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"

namespace kernel::arch::x86_64 {
using kernel::core::u16;
using kernel::core::u8;

static inline void outb(u16 port, u8 value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 value;

    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));

    return value;
}
}  // namespace kernel::arch::x86_64

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_IO_HPP_