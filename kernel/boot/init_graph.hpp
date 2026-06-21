#ifndef DOOM_OS_KERNEL_BOOT_INIT_GRAPH_HPP_
#define DOOM_OS_KERNEL_BOOT_INIT_GRAPH_HPP_

#include "kernel/boot/component.hpp"
#include "kernel/core/utils/traits.hpp"

namespace kernel::boot {

using kernel::core::false_type;
using kernel::core::integral_constant;
using kernel::core::is_same_v;
using kernel::core::true_type;

// =================================================================================================
// Utility
// =================================================================================================

template <typename>
inline constexpr bool always_false_v = false;

// =================================================================================================
// contains
// =================================================================================================

template <typename T, typename List>
struct contains;

template <typename T>
struct contains<T, type_list<>> : false_type {};

template <typename T, typename Head, typename... Tail>
struct contains<T, type_list<Head, Tail...>>
    : integral_constant<bool, is_same_v<T, Head> || contains<T, type_list<Tail...>>::value> {};

template <typename T, typename List>
inline constexpr bool contains_v = contains<T, List>::value;

// =================================================================================================
// push_back
// =================================================================================================

template <typename List, typename T>
struct push_back;

template <typename... Ts, typename T>
struct push_back<type_list<Ts...>, T> {
    using type = type_list<Ts..., T>;
};

template <typename List, typename T>
using push_back_t = typename push_back<List, T>::type;

// =================================================================================================
// push_back_unique
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

template <typename List, typename T>
struct push_back_unique {
    using type = conditional_t<
        contains_v<T, List>,
        List,
        push_back_t<List, T>
    >;
};

template <typename List, typename T>
using push_back_unique_t = typename push_back_unique<List, T>::type;

// =================================================================================================
// Compile-time topological sort
// =================================================================================================

template <typename Component, typename Sorted, typename Visiting>
struct topo_visit;

template <typename List, typename Sorted, typename Visiting>
struct topo_visit_list;

template <typename Sorted, typename Visiting>
struct topo_visit_list<type_list<>, Sorted, Visiting> {
    using type = Sorted;
};

template <typename Head, typename... Tail, typename Sorted, typename Visiting>
struct topo_visit_list<type_list<Head, Tail...>, Sorted, Visiting> {
    using sorted_head = typename topo_visit<Head, Sorted, Visiting>::type;

    using type = typename topo_visit_list<
        type_list<Tail...>,
        sorted_head,
        Visiting
    >::type;
};

template <
    typename Component,
    typename Sorted,
    typename Visiting,
    bool AlreadySorted,
    bool IsCycle
>
struct topo_visit_impl;

template <typename Component, typename Sorted, typename Visiting, bool IsCycle>
struct topo_visit_impl<Component, Sorted, Visiting, true, IsCycle> {
    using type = Sorted;
};

template <typename Component, typename Sorted, typename Visiting>
struct topo_visit_impl<Component, Sorted, Visiting, false, true> {
    static_assert(
        always_false_v<Component>,
        "cycle in kernel init dependency graph"
    );

    using type = Sorted;
};

template <typename Component, typename Sorted, typename Visiting>
struct topo_visit_impl<Component, Sorted, Visiting, false, false> {
    using visiting_with_component = push_back_t<Visiting, Component>;

    using sorted_deps = typename topo_visit_list<
        typename Component::deps,
        Sorted,
        visiting_with_component
    >::type;

    using type = push_back_unique_t<sorted_deps, Component>;
};

template <typename Component, typename Sorted, typename Visiting>
struct topo_visit
    : topo_visit_impl<
          Component,
          Sorted,
          Visiting,
          contains_v<Component, Sorted>,
          contains_v<Component, Visiting>
      > {};

template <typename Roots>
struct topo_sort;

template <typename... Roots>
struct topo_sort<type_list<Roots...>> {
    using type = typename topo_visit_list<
        type_list<Roots...>,
        type_list<>,
        type_list<>
    >::type;
};

template <typename Roots>
using topo_sort_t = typename topo_sort<Roots>::type;

} // namespace kernel::boot

#endif // DOOM_OS_KERNEL_BOOT_INIT_GRAPH_HPP_