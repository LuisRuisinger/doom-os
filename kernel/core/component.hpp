#ifndef DOOM_OS_KERNEL_CORE_COMPONENT_HPP_
#define DOOM_OS_KERNEL_CORE_COMPONENT_HPP_

// =================================================================================================
// Cpp stdlib files
// =================================================================================================

#include <type_traits>

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/result.hpp"
#include "kernel/core/types.hpp"

namespace kernel::core {
// =================================================================================================
// Type list
// =================================================================================================

template <typename... Ts>
struct type_list {};

using no_deps = type_list<>;

// =================================================================================================
// Init result
//
// What a component reports when it cannot publish its resource. The categories are shared
// across the graph so the runner can log any component's failure without knowing which one
// it ran; a component with a richer error domain of its own should keep that domain private
// and convert at the boundary with RESULT_ERROR_CONVERSION.
// =================================================================================================

enum class init_error : u8 {
    UNSPECIFIED,
    DEPENDENCY_UNAVAILABLE,
    INVALID_BOOT_DATA,
    CAPACITY_EXCEEDED,
    NO_USABLE_MEMORY,
    METADATA_STORAGE_UNAVAILABLE,
    RESERVATION_FAILED,
    HARDWARE_UNSUPPORTED,
};

inline const char *describe(init_error error)
{
    switch (error) {
        case init_error::UNSPECIFIED:
            return "unspecified failure";
        case init_error::DEPENDENCY_UNAVAILABLE:
            return "a dependency published no usable state";
        case init_error::INVALID_BOOT_DATA:
            return "boot data missing or malformed";
        case init_error::CAPACITY_EXCEEDED:
            return "input exceeds a compile-time capacity";
        case init_error::NO_USABLE_MEMORY:
            return "no usable memory in the boot memory map";
        case init_error::METADATA_STORAGE_UNAVAILABLE:
            return "no region large enough to hold allocator metadata";
        case init_error::RESERVATION_FAILED:
            return "could not reserve a required physical range";
        case init_error::HARDWARE_UNSUPPORTED:
            return "required hardware feature is unavailable";
    }

    return "unknown error";
}

using init_result = Result<void, init_error>;

// =================================================================================================
// Backing stores
// =================================================================================================

// Backing store for components that own no per-core state.
struct no_context {};

inline no_context no_backing{};

// A component that initialises hardware but owns no resource of its own.
struct no_resource {};

// =================================================================================================
// Resource binding
//
// Where a component's resource lives inside a backing store. The primary template is
// intentionally undefined: every binding is declared once, in the platform wiring header,
// so that a module never has to know the shape of the backing store it runs on.
// =================================================================================================

template <typename Component, typename Backing>
struct resource_binding;

// =================================================================================================
// Resource view
//
// The only handle a component receives on the backing store. It carries no access rules of
// its own; component<> below decides what may be reached through it.
// =================================================================================================

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

// =================================================================================================
// Component
//
// A component declares the resource it owns and the components it depends on by deriving
// from this base. In return it inherits the two accessors that can reach the backing store:
//
//   own(view)        its own resource, mutable
//   dep<D>(view)     a declared dependency's resource, const
//
// Reaching for anything else is a compile error. Because a dependency is only ever visible
// as const, a component can never be left half-initialised by someone else: whatever it
// publishes when init() returns is what every later component sees.
// =================================================================================================

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

}  // namespace kernel::core

#endif  // DOOM_OS_KERNEL_CORE_COMPONENT_HPP_
