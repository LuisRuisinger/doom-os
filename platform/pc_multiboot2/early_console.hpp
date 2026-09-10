#ifndef DOOM_OS_PLATFORM_PC_MULTIBOOT2_EARLY_CONSOLE_HPP_
#define DOOM_OS_PLATFORM_PC_MULTIBOOT2_EARLY_CONSOLE_HPP_

// =================================================================================================
// Architecture files
// =================================================================================================

#include "arch/x86_64/serial/serial.hpp"

namespace kernel::platform::pc_multiboot2::early_console {

// The PC Multiboot2 platform uses the legacy COM1 serial port as its early diagnostic console.
using component = kernel::arch::x86_64::serial::component;

}  // namespace kernel::platform::pc_multiboot2::early_console

#endif  // DOOM_OS_PLATFORM_PC_MULTIBOOT2_EARLY_CONSOLE_HPP_
