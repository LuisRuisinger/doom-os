// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/debug/console.hpp"

namespace kernel::debug {

namespace {

console_sink m_sink = nullptr;

}  // namespace

// =================================================================================================
// Console sink
// =================================================================================================

void set_console_sink(console_sink sink)
{
    m_sink = sink;
}

console_sink current_console_sink()
{
    return m_sink;
}

bool console_ready()
{
    return m_sink != nullptr;
}

void console_write(const char *bytes, usize length)
{
    if (m_sink == nullptr || bytes == nullptr || length == 0)
        return;

    m_sink(bytes, length);
}

}  // namespace kernel::debug
