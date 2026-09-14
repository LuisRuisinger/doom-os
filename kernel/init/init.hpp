#ifndef DOOM_OS_KERNEL_INIT_INIT_HPP_
#define DOOM_OS_KERNEL_INIT_INIT_HPP_

#include "arch/x86_64/cpu/cpu.hpp"
#include "kernel/debug/kprint.hpp"
#include "kernel/init/component.hpp"
#include "kernel/init/init_graph.hpp"

namespace kernel::init {

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
        KPRINTLN("[init] {}: FAIL ({})", Component::name, error);
    }
};

template <typename Roots, typename Backing, typename Logger>
bool run_init_graph(Backing &backing, Logger &logger)
{
    return detail::init_graph<Roots>::run_logged(backing, logger);
}

template <typename Roots, typename SatisfiedRoots, typename Backing, typename Logger>
bool run_init_graph_after(Backing &backing, Logger &logger)
{
    return detail::init_graph_after<Roots, SatisfiedRoots>::run_logged(backing, logger);
}

template <typename Roots, typename Backing = no_context>
bool run_init_graph_silent(Backing &backing = no_backing)
{
    return detail::init_graph<Roots>::run_silent(backing);
}

template <typename Roots, typename SatisfiedRoots, typename Backing = no_context>
bool run_init_graph_after_silent(Backing &backing = no_backing)
{
    return detail::init_graph_after<Roots, SatisfiedRoots>::run_silent(backing);
}

template <typename Roots, typename Backing = no_context>
void run_init_graph_or_halt(Backing &backing = no_backing)
{
    default_init_logger logger{};
    if (run_init_graph<Roots>(backing, logger))
        return;

    arch::x86_64::cpu::halt();
}

}  // namespace kernel::init

#endif  // DOOM_OS_KERNEL_INIT_INIT_HPP_
