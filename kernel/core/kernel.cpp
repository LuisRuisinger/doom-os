// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/boot_info.hpp"
#include "kernel/boot/boot_plan.hpp"
#include "kernel/core/init.hpp"
#include "kernel/core/types.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/debug/kprint.hpp"

// =================================================================================================
// Application
//
// The application is whatever defines main, so that a C program - Doom's i_main.c included - links
// unedited. Declared at global scope rather than inside a namespace, where it would be an
// unrelated function that happens to share a name.
//
// Nothing calls it on the way in: the image entry is _start32, per the linker script, so main is
// an ordinary symbol the kernel calls once, here. libos supplies a weak one for images with no
// application.
// =================================================================================================

int main(int argc, char **argv);

namespace kernel::core {

// =================================================================================================
// Kernel longmode entry point
// =================================================================================================

void kernel_main64(u64 mb2_magic, u64 mb2_info)
{
    kernel::boot::boot_info::set_handoff(mb2_magic, mb2_info);

    kernel::core::run_init_graph_silent<kernel::boot::early_boot_roots>();
    KPRINTLN("KERNEL BOOT");

    kernel::core::run_init_graph_or_halt<kernel::boot::platform_roots>();

    // Global constructors are the last node of this graph rather than a call after it - see
    // kernel::runtime::component. By the time one runs it is entitled to everything main is: SSE
    // legal, so newlib's string routines do not fault, and a heap behind it, so an allocating
    // constructor reaches a live PMM.
    kernel::core::run_init_graph_or_halt<kernel::boot::boot_roots>();

    // Zero arguments rather than a fabricated argv[0]. An application wanting a command line
    // should take the real one from boot info, not a name this kernel invented. An application
    // declaring main(void) is called through a wider prototype here, exactly as every C runtime
    // startup on this ABI does: the arguments arrive in registers the callee does not read.
    KPRINTLN("[app] returned {}", main(0, nullptr));

    asm volatile("ud2");
}

}  // namespace kernel::core

// =================================================================================================
// C ABI wrapper
// =================================================================================================

extern "C" void kernel_main64(kernel::core::u64 mb2_magic, kernel::core::u64 mb2_info)
{
    kernel::core::kernel_main64(mb2_magic, mb2_info);
}
