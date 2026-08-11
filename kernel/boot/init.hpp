#ifndef DOOM_OS_KERNEL_BOOT_INIT_HPP_
#define DOOM_OS_KERNEL_BOOT_INIT_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/component.hpp"
#include "kernel/boot/init_graph.hpp"
#include "kernel/debug/kprint.hpp"

namespace kernel::boot {
// =================================================================================================
// Default logger
// =================================================================================================

struct default_init_logger {
    template <typename Component, typename Backing>
    void begin(Backing &)
    {
        KPRINTLN("[init] {}", Component::name);
    }

    template <typename Component, typename Backing>
    void ok(Backing &)
    {
        KPRINTLN("[init] {}: OK", Component::name);
    }

    template <typename Component, typename Backing>
    void fail(Backing &, init_error error)
    {
        KPRINTLN("[init] {}: FAIL ({})", Component::name, describe(error));
    }
};

// =================================================================================================
// Halt
// =================================================================================================

[[noreturn]] inline void halt_forever()
{
    for (;;) {
        asm volatile("cli; hlt");
    }
}

// =================================================================================================
// Init graph
//
// Components that own no per-core state run against the shared empty backing store, so the
// same three entry points cover both the boot graph and the per-core graphs.
// =================================================================================================

template <typename Roots, typename Backing, typename Logger>
bool run_init_graph(Backing &backing, Logger &logger)
{
    return detail::init_graph<Roots>::run_logged(backing, logger);
}

template <typename Roots, typename Backing = no_context>
bool run_init_graph_silent(Backing &backing = no_backing)
{
    return detail::init_graph<Roots>::run_silent(backing);
}

template <typename Roots, typename Backing = no_context>
void run_init_graph_or_halt(Backing &backing = no_backing)
{
    default_init_logger logger{};

    if (run_init_graph<Roots>(backing, logger)) {
        return;
    }

    halt_forever();
}

}  // namespace kernel::boot

#endif  // DOOM_OS_KERNEL_BOOT_INIT_HPP_
