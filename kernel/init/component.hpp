#ifndef DOOM_OS_KERNEL_INIT_COMPONENT_HPP_
#define DOOM_OS_KERNEL_INIT_COMPONENT_HPP_

#include <type_traits>

#include "kernel/core/result.hpp"
#include "kernel/core/types.hpp"

namespace kernel::init {

using kernel::core::Result;
using kernel::core::u8;

template <typename... Ts>
struct type_list {};

using no_deps = type_list<>;

enum class init_error : u8 {
    UNSPECIFIED,
    DEPENDENCY_UNAVAILABLE,
    INVALID_BOOT_DATA,
    CAPACITY_EXCEEDED,
    NO_USABLE_MEMORY,
    HARDWARE_UNSUPPORTED,
};

using init_result = Result<void, init_error>;

struct no_context {};

inline no_context no_backing{};

struct no_resource {};

template <typename Component, typename Backing>
struct resource_binding;

template <typename Backing>
class resource_view {
    Backing &backing_m;

    template <typename, typename, typename...>
    friend struct component;

    template <typename Component>
    auto &bind() const
    {
        return resource_binding<Component, Backing>::get(backing_m);
    }

public:
    explicit resource_view(Backing &backing)
        : backing_m(backing)
    {
    }
};

template <typename Self, typename Resource, typename... Deps>
struct component {
    using self = Self;
    using resource = Resource;
    using deps = type_list<Deps...>;

protected:
    template <typename View>
    static Resource &own(View view)
    {
        static_assert(!std::is_same_v<Resource, no_resource>,
                      "component owns no resource: declare one to use own()");

        return view.template bind<Self>();
    }

    template <typename Dep, typename View>
    static const auto &dep(View view)
    {
        static_assert((std::is_same_v<Dep, Deps> || ...),
                      "component declares no dependency on the component whose resource it "
                      "is trying to read");

        return view.template bind<Dep>();
    }
};

}  // namespace kernel::init

#endif  // DOOM_OS_KERNEL_INIT_COMPONENT_HPP_
