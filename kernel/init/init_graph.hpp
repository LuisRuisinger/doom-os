#ifndef DOOM_OS_KERNEL_INIT_INIT_GRAPH_HPP_
#define DOOM_OS_KERNEL_INIT_INIT_GRAPH_HPP_

#include <type_traits>

#include "kernel/init/component.hpp"

namespace kernel::init::detail {

template <typename T, typename List>
struct list_contains;

template <typename T>
struct list_contains<T, type_list<>> {
    static constexpr bool value = false;
};

template <typename T, typename Head, typename... Tail>
struct list_contains<T, type_list<Head, Tail...>> {
    static constexpr bool value =
        std::is_same_v<T, Head> || list_contains<T, type_list<Tail...>>::value;
};

template <typename List, typename T>
struct list_append;

template <typename... Ts, typename T>
struct list_append<type_list<Ts...>, T> {
    using type = type_list<Ts..., T>;
};

template <typename List, typename T>
struct list_append_unique {
    using type = std::conditional_t<list_contains<T, List>::value, List,
                                    typename list_append<List, T>::type>;
};

template <typename A, typename B>
struct list_concat_unique;

template <typename A>
struct list_concat_unique<A, type_list<>> {
    using type = A;
};

template <typename A, typename Head, typename... Tail>
struct list_concat_unique<A, type_list<Head, Tail...>> {
    using with_head = typename list_append_unique<A, Head>::type;

    using type = typename list_concat_unique<with_head, type_list<Tail...>>::type;
};

template <typename List, typename Excluded, typename Accum>
struct list_filter_out_accum;

template <typename Excluded, typename Accum>
struct list_filter_out_accum<type_list<>, Excluded, Accum> {
    using type = Accum;
};

template <typename Head, typename... Tail, typename Excluded, typename Accum>
struct list_filter_out_accum<type_list<Head, Tail...>, Excluded, Accum> {
    using with_head = std::conditional_t<list_contains<Head, Excluded>::value, Accum,
                                         typename list_append<Accum, Head>::type>;

    using type = typename list_filter_out_accum<type_list<Tail...>, Excluded, with_head>::type;
};

template <typename List, typename Excluded>
struct list_filter_out {
    using type = typename list_filter_out_accum<List, Excluded, type_list<>>::type;
};

template <typename Component, typename Visiting>
struct topo_component;

template <typename List, typename Visiting>
struct topo_list;

template <typename Visiting>
struct topo_list<type_list<>, Visiting> {
    using type = type_list<>;
};

template <typename Head, typename... Tail, typename Visiting>
struct topo_list<type_list<Head, Tail...>, Visiting> {
    using head_sorted = typename topo_component<Head, Visiting>::type;
    using tail_sorted = typename topo_list<type_list<Tail...>, Visiting>::type;

    using type = typename list_concat_unique<head_sorted, tail_sorted>::type;
};

template <typename Component, typename Visiting>
struct topo_component {
    static constexpr bool is_cyclic = list_contains<Component, Visiting>::value;

    static_assert(!is_cyclic, "cycle detected in init graph");

    using visiting_with_self = typename list_append<Visiting, Component>::type;

    using deps_sorted =
        typename topo_list<std::conditional_t<is_cyclic, type_list<>, typename Component::deps>,
                           visiting_with_self>::type;

    using type = typename list_append_unique<deps_sorted, Component>::type;
};

template <typename Component, typename Backing>
bool run_component_silent(Backing &backing)
{
    return Component::init(resource_view<Backing>{backing}).is_ok();
}

template <typename Component, typename Backing, typename Logger>
bool run_component_logged(Backing &backing, Logger &logger)
{
    logger.template begin<Component>(backing);

    init_result outcome = Component::init(resource_view<Backing>{backing});

    if (outcome.is_err()) {
        logger.template fail<Component>(backing, outcome.unwrap_err_ref());
        return false;
    }

    logger.template ok<Component>(backing);
    return true;
}

template <typename List>
struct init_list_runner;

template <typename... Components>
struct init_list_runner<type_list<Components...>> {
    template <typename Backing>
    static bool run_silent(Backing &backing)
    {
        return (run_component_silent<Components>(backing) && ...);
    }

    template <typename Backing, typename Logger>
    static bool run_logged(Backing &backing, Logger &logger)
    {
        return (run_component_logged<Components>(backing, logger) && ...);
    }
};

template <typename Roots>
struct init_graph {
    using sorted_components = typename topo_list<Roots, type_list<>>::type;

    template <typename Backing>
    static bool run_silent(Backing &backing)
    {
        return init_list_runner<sorted_components>::run_silent(backing);
    }

    template <typename Backing, typename Logger>
    static bool run_logged(Backing &backing, Logger &logger)
    {
        return init_list_runner<sorted_components>::run_logged(backing, logger);
    }
};

template <typename Roots, typename SatisfiedRoots>
struct init_graph_after {
    using sorted_components = typename topo_list<Roots, type_list<>>::type;
    using satisfied_components = typename topo_list<SatisfiedRoots, type_list<>>::type;
    using runnable_components =
        typename list_filter_out<sorted_components, satisfied_components>::type;

    template <typename Backing>
    static bool run_silent(Backing &backing)
    {
        return init_list_runner<runnable_components>::run_silent(backing);
    }

    template <typename Backing, typename Logger>
    static bool run_logged(Backing &backing, Logger &logger)
    {
        return init_list_runner<runnable_components>::run_logged(backing, logger);
    }
};

}  // namespace kernel::init::detail

#endif  // DOOM_OS_KERNEL_INIT_INIT_GRAPH_HPP_
