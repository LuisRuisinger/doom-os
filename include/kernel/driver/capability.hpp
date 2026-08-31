#ifndef DOOM_OS_KERNEL_INCLUDE_KERNEL_DRIVER_CAPABILITY_HPP_
#define DOOM_OS_KERNEL_INCLUDE_KERNEL_DRIVER_CAPABILITY_HPP_

// =================================================================================================
// Kernel public files
// =================================================================================================

#include <kernel/driver/registry.hpp>

namespace kernel::driver {

// =================================================================================================
// Capability declarations
//
// A capability type is a stable API contract. It is intentionally separate from a driver
// dependency: dependencies order initialization, while capabilities are what later code asks for.
// =================================================================================================

template <typename Capability>
concept capability_definition = requires {
    typename Capability::api;
    Capability::name;
};

namespace detail {

template <typename Capability>
consteval capability_multiplicity capability_multiplicity_of()
{
    if constexpr (requires { Capability::multiplicity; }) {
        return Capability::multiplicity;
    } else {
        return capability_multiplicity::SINGLE;
    }
}

[[noreturn]] void panic_missing_capability(const capability_key &key);

}  // namespace detail

template <capability_definition Capability>
inline constinit const capability_key capability_key_for{
    Capability::name,
    detail::capability_multiplicity_of<Capability>(),
};

const void *find_capability(const capability_key &key);

template <capability_definition Capability>
const typename Capability::api *try_get()
{
    return static_cast<const typename Capability::api *>(
        find_capability(capability_key_for<Capability>));
}

template <capability_definition Capability>
const typename Capability::api &get()
{
    const typename Capability::api *api = try_get<Capability>();

    if (api == nullptr) {
        detail::panic_missing_capability(capability_key_for<Capability>);
    }

    return *api;
}

}  // namespace kernel::driver

#endif  // DOOM_OS_KERNEL_INCLUDE_KERNEL_DRIVER_CAPABILITY_HPP_
