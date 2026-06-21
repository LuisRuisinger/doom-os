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
// Component base
// =================================================================================================

template <typename Derived, typename Deps = no_deps>
struct component_base {
    using self = Derived;
    using deps = Deps;

    static constexpr bool dump_debug_state_on_failure = false;

    static bool run() {
        return Derived::init_component();
    }
};

} // namespace kernel::boot

#endif // DOOM_OS_KERNEL_BOOT_COMPONENT_HPP_