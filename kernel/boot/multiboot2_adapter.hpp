#ifndef DOOM_OS_KERNEL_BOOT_MULTIBOOT2_ADAPTER_HPP_
#define DOOM_OS_KERNEL_BOOT_MULTIBOOT2_ADAPTER_HPP_

#include "kernel/boot/multiboot2.hpp"
#include "kernel/boot/protocol.hpp"

namespace kernel::boot::multiboot2 {

struct adapter {
    static constexpr auto *name = "multiboot2";

    static kernel::init::init_result parse(const kernel::boot::handoff &source,
                                           kernel::boot::info          &out);
};

static_assert(kernel::boot::boot_protocol<adapter>);

}  // namespace kernel::boot::multiboot2

#endif  // DOOM_OS_KERNEL_BOOT_MULTIBOOT2_ADAPTER_HPP_
