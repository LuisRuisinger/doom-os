// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/debug/console.hpp"

namespace kernel::debug {

namespace {

console_sink g_sink = nullptr;

}  // namespace

// =================================================================================================
// Console sink
// =================================================================================================

void set_console_sink(console_sink sink)
{
    g_sink = sink;
}

console_sink current_console_sink()
{
    return g_sink;
}

bool console_ready()
{
    return g_sink != nullptr;
}

void console_write(const char *bytes, usize length)
{
    if (g_sink == nullptr || bytes == nullptr || length == 0)
        return;

    g_sink(bytes, length);
}

}  // namespace kernel::debug
