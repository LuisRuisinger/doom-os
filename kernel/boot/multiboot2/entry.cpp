// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/multiboot2/adapter.hpp"
#include "kernel/kernel.hpp"

namespace kernel::boot::multiboot2 {

// Jumped to from arch/x86_64/boot/multiboot2/longmode.S.
[[noreturn]] void entry(u64 magic, u64 info) asm("kernel_multiboot2_entry");

void entry(u64 magic, u64 info)
{
    set_handoff(magic, info);
    kernel::kernel_main();
}

}  // namespace kernel::boot::multiboot2
