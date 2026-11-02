#include "kernel/kernel.hpp"

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/debug/kpanic.hpp"
#include "kernel/debug/kprint.hpp"
#include "kernel/init/init.hpp"
#include "platform/platform.hpp"

extern "C" [[noreturn]] void doom_os_start_application();

namespace kernel {

void kernel_main()
{
    kernel::init::run_init_graph_silent<kernel::platform::early_boot_roots>();
    KPRINTLN("KERNEL BOOT");

    kernel::init::run_init_graph_or_halt<kernel::platform::platform_roots>();

    kernel::init::run_init_graph_or_halt<kernel::platform::boot_roots>();

    doom_os_start_application();

    // just for kpanic testing
    asm volatile("ud2");
}

}  // namespace kernel
