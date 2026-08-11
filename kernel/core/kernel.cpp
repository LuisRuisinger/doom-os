// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/boot_info.hpp"
#include "kernel/boot/boot_plan.hpp"
#include "kernel/boot/init.hpp"
#include "kernel/core/types.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/debug/kprint.hpp"
#include "kernel/runtime/init.hpp"

namespace kernel::core {

// =================================================================================================
// Kernel longmode entry point
// =================================================================================================

void kernel_main64(u64 mb2_magic, u64 mb2_info)
{
    kernel::boot::boot_info::set_handoff(mb2_magic, mb2_info);

    kernel::boot::run_init_graph_silent<kernel::boot::early_boot_roots>();
    KPRINTLN("KERNEL BOOT");

    kernel::runtime::call_global_constructors();

    kernel::boot::run_init_graph_or_halt<kernel::boot::platform_roots>();
    kernel::boot::run_init_graph_or_halt<kernel::boot::boot_roots>();

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
