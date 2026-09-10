#ifndef DOOM_OS_PLATFORM_PC_MULTIBOOT2_BOOT_WIRING_HPP_
#define DOOM_OS_PLATFORM_PC_MULTIBOOT2_BOOT_WIRING_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/multiboot2_adapter.hpp"
#include "kernel/boot/protocol.hpp"

namespace kernel::boot {

// =================================================================================================
// Active boot protocol
//
// The one place that names a boot protocol. Supporting another one means writing an adapter
// and changing this line; no consumer of the boot description has to be touched.
// =================================================================================================

using active_boot_protocol = kernel::boot::multiboot2::adapter;

static_assert(boot_protocol<active_boot_protocol>);

}  // namespace kernel::boot

#endif  // DOOM_OS_PLATFORM_PC_MULTIBOOT2_BOOT_WIRING_HPP_
