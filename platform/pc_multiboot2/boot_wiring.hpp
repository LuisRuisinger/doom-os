#ifndef DOOM_OS_PLATFORM_PC_MULTIBOOT2_BOOT_WIRING_HPP_
#define DOOM_OS_PLATFORM_PC_MULTIBOOT2_BOOT_WIRING_HPP_

#include "kernel/boot/multiboot2_adapter.hpp"
#include "kernel/boot/protocol.hpp"

namespace kernel::boot {

using active_boot_protocol = kernel::boot::multiboot2::adapter;

static_assert(boot_protocol<active_boot_protocol>);

}  // namespace kernel::boot

#endif  // DOOM_OS_PLATFORM_PC_MULTIBOOT2_BOOT_WIRING_HPP_
