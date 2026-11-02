#ifndef DOOM_OS_KERNEL_BOOT_MULTIBOOT2_ADAPTER_HPP_
#define DOOM_OS_KERNEL_BOOT_MULTIBOOT2_ADAPTER_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/multiboot2/spec.hpp"
#include "kernel/boot/protocol.hpp"

namespace kernel::boot::multiboot2 {

void set_handoff(u64 magic, kernel::core::paddr_t address);

struct adapter {
    static constexpr const char *name = "multiboot2";

    [[nodiscard]] static bool well_formed();

    static kernel::init::init_result parse(kernel::boot::info &out);
};

static_assert(kernel::boot::boot_protocol<adapter>);

}  // namespace kernel::boot::multiboot2

#endif  // DOOM_OS_KERNEL_BOOT_MULTIBOOT2_ADAPTER_HPP_
