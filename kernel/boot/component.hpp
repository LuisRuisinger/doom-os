#ifndef DOOM_OS_KERNEL_BOOT_COMPONENT_HPP_
#define DOOM_OS_KERNEL_BOOT_COMPONENT_HPP_

namespace kernel::boot {
// =================================================================================================
// Type list
// =================================================================================================

template <typename... Ts>
struct type_list {};

using no_deps = type_list<>;

// =================================================================================================
// Context
// =================================================================================================

struct no_context {};

// =================================================================================================
// Component base
// =================================================================================================

template <typename Derived, typename Deps = no_deps>
struct component_base {
    using self = Derived;
    using deps = Deps;
    using context = no_context;

    static constexpr bool dump_debug_state_on_failure = false;

    template <typename Context>
    static bool run(Context &) {
        return Derived::init_component();
    }
};

// =================================================================================================
// Context component base
// =================================================================================================

template <typename Derived, typename Context, typename Deps = no_deps>
struct context_component_base {
    using self = Derived;
    using deps = Deps;
    using context = Context;

    static constexpr bool dump_debug_state_on_failure = false;

    static bool run(Context &context) { return Derived::init_component(context); }
};

}  // namespace kernel::boot

#endif  // DOOM_OS_KERNEL_BOOT_COMPONENT_HPP_
