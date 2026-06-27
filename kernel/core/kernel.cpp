// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/boot_plan.hpp"
#include "kernel/boot/init.hpp"
#include "kernel/core/types.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/debug/kprint.hpp"

namespace kernel::core {
static constexpr auto *BOOT_BANNER =
    R"banner(
============================================================
  DDDDD    OOOOO    OOOOO   MM     MM   OOOOO    SSSSS
  DD  DD  OO   OO  OO   OO  MMM   MMM  OO   OO  SS
  DD  DD  OO   OO  OO   OO  MM M M MM  OO   OO   SSSS
  DD  DD  OO   OO  OO   OO  MM  M  MM  OO   OO      SS
  DDDDD    OOOOO    OOOOO   MM     MM   OOOOO   SSSSS

                    DoomOS kernel
============================================================
)banner";

// =================================================================================================
// Kernel longmode entry point
// =================================================================================================

void kernel_main64(u64 mb2_magic [[maybe_unused]], u64 mb2_info [[maybe_unused]]) {
    kernel::boot::run_init_graph_silent<kernel::boot::early_boot_roots>();
    KPRINTLN("kernel boot");
    // KPRINTLN("[boot] mb2_magic={:x}", mb2_magic);
    // KPRINTLN("[boot] mb2_info={:x}", mb2_info);

    kernel::boot::run_init_graph_or_halt<kernel::boot::boot_roots>();
    // KPRINTLN("\n{}", BOOT_BANNER);

    KPANIC();
    for (;;) {
        asm volatile("hlt");
    }
}
}  // namespace kernel::core

// =================================================================================================
// C ABI wrapper
// =================================================================================================

extern "C" void kernel_main64(kernel::core::u64 mb2_magic, kernel::core::u64 mb2_info) {
    kernel::core::kernel_main64(mb2_magic, mb2_info);
}