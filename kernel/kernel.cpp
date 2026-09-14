// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/boot_info.hpp"
#include "kernel/core/types.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/debug/kprint.hpp"
#include "kernel/init/init.hpp"
#include "platform/pc_multiboot2/boot_plan.hpp"

// =================================================================================================
// Application
//
// The kernel calls into libos rather than naming a language entry point directly. libos owns the
// application lifecycle: no-app handling, calling C ABI main for app images, and turning a returned
// status into _exit.
// =================================================================================================

extern "C" [[noreturn]] void doom_os_start_application();

namespace kernel {

using kernel::core::u64;

namespace boot_platform = kernel::platform::pc_multiboot2;

// =================================================================================================
// Kernel longmode entry point
// =================================================================================================

void kernel_main64(u64 mb2_magic, u64 mb2_info)
{
    kernel::boot::boot_info::set_handoff(mb2_magic, mb2_info);

    kernel::init::run_init_graph_silent<boot_platform::early_boot_roots>();
    KPRINTLN("KERNEL BOOT");

    kernel::init::run_init_graph_or_halt<boot_platform::platform_roots>();

    // Global constructors are the last node of this graph rather than a call after it - see
    // kernel::runtime::component. By the time one runs it is entitled to everything main is: SSE
    // legal, so a C library's string routines do not fault, and a heap behind it, so an allocating
    // constructor reaches a live PMM.
    kernel::init::run_init_graph_or_halt<boot_platform::boot_roots>();

    doom_os_start_application();

    // just for kpanic testing
    asm volatile("ud2");
}

}  // namespace kernel

// =================================================================================================
// C ABI wrapper
// =================================================================================================

extern "C" void kernel_main64(kernel::core::u64 mb2_magic, kernel::core::u64 mb2_info)
{
    kernel::kernel_main64(mb2_magic, mb2_info);
}
