#ifndef DOOM_OS_KERNEL_BOOT_INIT_GRAPH_HPP_
#define DOOM_OS_KERNEL_BOOT_INIT_GRAPH_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/component.hpp"

namespace kernel::boot::detail {
// =================================================================================================
// Integral constant
// =================================================================================================

template <typename T, T Value>
struct integral_constant {
    static constexpr T value = Value;

    using value_type = T;
    using type = integral_constant<T, Value>;

    constexpr operator value_type() const noexcept { return value; }
};

using true_type = integral_constant<bool, true>;
using false_type = integral_constant<bool, false>;

// =================================================================================================
// Is same
// =================================================================================================

template <typename A, typename B>
struct is_same : false_type {};

template <typename A>
struct is_same<A, A> : true_type {};

template <typename A, typename B>
inline constexpr bool is_same_v = is_same<A, B>::value;

// =================================================================================================
// Conditional
// =================================================================================================

template <bool Condition, typename TrueType, typename FalseType>
struct conditional {
    using type = TrueType;
};

template <typename TrueType, typename FalseType>
struct conditional<false, TrueType, FalseType> {
    using type = FalseType;
};

template <bool Condition, typename TrueType, typename FalseType>
using conditional_t = typename conditional<Condition, TrueType, FalseType>::type;

// =================================================================================================
// Type list helpers
// =================================================================================================

template <typename T, typename List>
struct list_contains;

template <typename T>
struct list_contains<T, type_list<> > {
    static constexpr bool value = false;
};

template <typename T, typename Head, typename... Tail>
struct list_contains<T, type_list<Head, Tail...> > {
    static constexpr bool value =
        is_same_v<T, Head> || list_contains<T, type_list<Tail...> >::value;
};

template <typename List, typename T>
struct list_append;

template <typename... Ts, typename T>
struct list_append<type_list<Ts...>, T> {
    using type = type_list<Ts..., T>;
};

template <typename List, typename T>
struct list_append_unique {
    using type =
        conditional_t<list_contains<T, List>::value, List, typename list_append<List, T>::type>;
};

template <typename A, typename B>
struct list_concat_unique;

template <typename A>
struct list_concat_unique<A, type_list<> > {
    using type = A;
};

template <typename A, typename Head, typename... Tail>
struct list_concat_unique<A, type_list<Head, Tail...> > {
    using with_head = typename list_append_unique<A, Head>::type;

    using type = typename list_concat_unique<with_head, type_list<Tail...> >::type;
};

// =================================================================================================
// Topological sort
// =================================================================================================

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
    static_assert(!list_contains<Component, Visiting>::value, "cycle detected in init graph");

    using visiting_with_self = typename list_append<Visiting, Component>::type;
    using deps_sorted = typename topo_list<typename Component::deps, visiting_with_self>::type;

    using type = typename list_append_unique<deps_sorted, Component>::type;
};

// =================================================================================================
// Sorted list runner
// =================================================================================================

template <typename Component, typename Context>
bool run_component_silent(Context &context) {
    return Component::run(context);
}

template <typename Component, typename Context, typename Logger>
bool run_component_logged(Context &context, Logger &logger) {
    logger.template begin<Component>(context);

    if (!Component::run(context)) {
        logger.template fail<Component>(context);
        return false;
    }

    logger.template ok<Component>(context);
    return true;
}

template <typename List>
struct init_list_runner;

template <typename... Components>
struct init_list_runner<type_list<Components...> > {
    template <typename Context>
    static bool run_silent(Context &context) {
        return (run_component_silent<Components>(context) && ...);
    }

    template <typename Context, typename Logger>
    static bool run_logged(Context &context, Logger &logger) {
        return (run_component_logged<Components>(context, logger) && ...);
    }
};

// =================================================================================================
// Init graph
// =================================================================================================

template <typename Roots>
struct init_graph {
    using sorted_components = typename topo_list<Roots, type_list<> >::type;

    template <typename Context>
    static bool run_silent(Context &context) {
        return init_list_runner<sorted_components>::run_silent(context);
    }

    template <typename Context, typename Logger>
    static bool run_logged(Context &context, Logger &logger) {
        return init_list_runner<sorted_components>::run_logged(context, logger);
    }
};
}  // namespace kernel::boot::detail

#endif  // DOOM_OS_KERNEL_BOOT_INIT_GRAPH_HPP_