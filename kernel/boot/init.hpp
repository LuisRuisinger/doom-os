#ifndef DOOM_OS_KERNEL_BOOT_INIT_HPP_
#define DOOM_OS_KERNEL_BOOT_INIT_HPP_

#include "kernel/boot/init_graph.hpp"
#include "kernel/debug/kprint.hpp"

namespace kernel::boot {

// =================================================================================================
// Init failure policy
// =================================================================================================

[[noreturn]] inline void halt_on_init_failure() {
    for (;;) {
        asm volatile("cli; hlt");
    }
}

// =================================================================================================
// Component runner
// =================================================================================================

template <typename Component>
bool run_component_logged() {
    KPRINTLN("[init] {}", Component::name);

    if (!Component::run()) {
        KPRINTLN("[init] {}: FAILED", Component::name);
        return false;
    }

    KPRINTLN("[init] {}: OK", Component::name);
    return true;
}

template <typename Component>
bool run_component_silent() {
    return Component::run();
}

// =================================================================================================
// Init runner
// =================================================================================================

template <typename List>
struct init_runner_logged;

template <typename... Components>
struct init_runner_logged<type_list<Components...>> {
    static bool run() {
        return (run_component_logged<Components>() && ...);
    }
};

template <typename List>
struct init_runner_silent;

template <typename... Components>
struct init_runner_silent<type_list<Components...>> {
    static bool run() {
        return (run_component_silent<Components>() && ...);
    }
};

template <typename Roots>
bool run_init_graph_silent() {
    using ordered_components = topo_sort_t<Roots>;
    return init_runner_silent<ordered_components>::run();
}

template <typename Roots>
bool run_init_graph_logged() {
    using ordered_components = topo_sort_t<Roots>;
    return init_runner_logged<ordered_components>::run();
}

template <typename Roots>
void run_init_graph_or_halt() {
    if (!run_init_graph_logged<Roots>()) {
        KPRINTLN("[init] kernel initialization failed");
        halt_on_init_failure();
    }
}

} // namespace kernel::boot

#endif // DOOM_OS_KERNEL_BOOT_INIT_HPP_