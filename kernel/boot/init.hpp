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
    template <typename Component, typename Context>
    void begin(Context &) {
        KPRINTLN("[init] {}", Component::name);
    }

    template <typename Component, typename Context>
    void ok(Context &) {
        KPRINTLN("[init] {}: OK", Component::name);
    }

    template <typename Component, typename Context>
    void fail(Context &) {
        KPRINTLN("[init] {}: FAIL", Component::name);
    }
};

// =================================================================================================
// Halt
// =================================================================================================

[[noreturn]] inline void halt_forever() {
    for (;;) {
        asm volatile("cli; hlt");
    }
}

// =================================================================================================
// Init graph with context
// =================================================================================================

template <typename Roots, typename Context>
bool run_init_graph_silent(Context &context) {
    return detail::init_graph<Roots>::run_silent(context);
}

template <typename Roots, typename Context, typename Logger>
bool run_init_graph_logged(Context &context, Logger &logger) {
    return detail::init_graph<Roots>::run_logged(context, logger);
}

template <typename Roots, typename Context>
bool run_init_graph_logged(Context &context) {
    default_init_logger logger{};
    return run_init_graph_logged<Roots>(context, logger);
}

template <typename Roots, typename Context>
void run_init_graph_or_halt(Context &context) {
    if (run_init_graph_logged<Roots>(context)) {
        return;
    }

    halt_forever();
}

// =================================================================================================
// Init graph without context
// =================================================================================================

template <typename Roots>
bool run_init_graph_silent() {
    no_context context{};
    return run_init_graph_silent<Roots>(context);
}

template <typename Roots>
bool run_init_graph_logged() {
    no_context context{};
    return run_init_graph_logged<Roots>(context);
}

template <typename Roots>
void run_init_graph_or_halt() {
    no_context context{};
    run_init_graph_or_halt<Roots>(context);
}
}  // namespace kernel::boot

#endif  // DOOM_OS_KERNEL_BOOT_INIT_HPP_