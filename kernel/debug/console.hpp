#ifndef DOOM_OS_KERNEL_DEBUG_CONSOLE_HPP_
#define DOOM_OS_KERNEL_DEBUG_CONSOLE_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"

namespace kernel::debug {

using kernel::core::usize;

// =================================================================================================
// Console sink
//
// Where formatted output ends up. A device installs itself once it is able to accept bytes,
// which inverts the old arrangement where the formatter reached into a specific UART and
// initialised it.
//
// Until something is installed, output is discarded. That is deliberate: printing before the
// device is configured used to write into an unconfigured 16550, which produces nothing
// useful and no indication that anything was lost.
// =================================================================================================

using console_sink = void (*)(const char *bytes, usize length);

void set_console_sink(console_sink sink);

[[nodiscard]] console_sink current_console_sink();

[[nodiscard]] bool console_ready();

// Write straight to the installed sink, bypassing formatting.
void console_write(const char *bytes, usize length);

}  // namespace kernel::debug

#endif  // DOOM_OS_KERNEL_DEBUG_CONSOLE_HPP_
