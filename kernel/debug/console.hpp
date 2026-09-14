#ifndef DOOM_OS_KERNEL_DEBUG_CONSOLE_HPP_
#define DOOM_OS_KERNEL_DEBUG_CONSOLE_HPP_

#include "kernel/core/types.hpp"

namespace kernel::debug {

using kernel::core::usize;

using console_sink = void (*)(const char *bytes, usize length);

void set_console_sink(console_sink sink);

[[nodiscard]] console_sink current_console_sink();

[[nodiscard]] bool console_ready();

void console_write(const char *bytes, usize length);

}  // namespace kernel::debug

#endif  // DOOM_OS_KERNEL_DEBUG_CONSOLE_HPP_
