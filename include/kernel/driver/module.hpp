#ifndef DOOM_OS_KERNEL_INCLUDE_KERNEL_DRIVER_MODULE_HPP_
#define DOOM_OS_KERNEL_INCLUDE_KERNEL_DRIVER_MODULE_HPP_

// =================================================================================================
// Kernel public files
// =================================================================================================

#include <kernel/driver/capability.hpp>
#include <kernel/driver/registry.hpp>

// =================================================================================================
// Driver declaration macros
//
// These macros are intentionally Linux-like: a driver translation unit declares a driver id,
// then contributes init/exit/dependency/capability records to linker-collected sections.
//
// Driver names must be plain C identifier tokens. Dependency names are stored as strings, so a
// driver can depend on another driver declared in a different translation unit.
// =================================================================================================

#define DOOM_OS_DRIVER_DETAIL_CONCAT_INNER(lhs__, rhs__) lhs__##rhs__
#define DOOM_OS_DRIVER_DETAIL_CONCAT(lhs__, rhs__) \
    DOOM_OS_DRIVER_DETAIL_CONCAT_INNER(lhs__, rhs__)

#define DOOM_OS_DRIVER_DETAIL_ID(name__) DOOM_OS_DRIVER_DETAIL_CONCAT(doom_os_driver_id_, name__)

#define DOOM_OS_DRIVER_DETAIL_SECTION(section__) \
    __attribute__((used, section(section__), aligned(8)))

#define DOOM_OS_DRIVER(name__)                                                        \
    static constinit const ::kernel::driver::driver_id DOOM_OS_DRIVER_DETAIL_ID(name__) \
        DOOM_OS_DRIVER_DETAIL_SECTION("doom_os_driver_ids") = {#name__}

#define DOOM_OS_DRIVER_INIT(name__, init_fn__)                                             \
    static constinit const ::kernel::driver::init_record                                   \
        DOOM_OS_DRIVER_DETAIL_CONCAT(__doom_os_driver_init_, __COUNTER__)                  \
            DOOM_OS_DRIVER_DETAIL_SECTION("doom_os_driver_init") = {                       \
                &DOOM_OS_DRIVER_DETAIL_ID(name__), init_fn__}

#define DOOM_OS_DRIVER_ENTRY(name__, entry_fn__) DOOM_OS_DRIVER_INIT(name__, entry_fn__)

#define DOOM_OS_DRIVER_EXIT(name__, exit_fn__)                                             \
    static constinit const ::kernel::driver::exit_record                                   \
        DOOM_OS_DRIVER_DETAIL_CONCAT(__doom_os_driver_exit_, __COUNTER__)                  \
            DOOM_OS_DRIVER_DETAIL_SECTION("doom_os_driver_exit") = {                       \
                &DOOM_OS_DRIVER_DETAIL_ID(name__), exit_fn__}

#define DOOM_OS_DRIVER_REQUIRES_DRIVER(name__, dependency__)                               \
    static constinit const ::kernel::driver::require_driver_record                         \
        DOOM_OS_DRIVER_DETAIL_CONCAT(__doom_os_driver_requires_driver_, __COUNTER__)        \
            DOOM_OS_DRIVER_DETAIL_SECTION("doom_os_driver_requires_driver") = {            \
                &DOOM_OS_DRIVER_DETAIL_ID(name__), #dependency__}

#define DOOM_OS_DRIVER_DEPENDS_ON(name__, dependency__) \
    DOOM_OS_DRIVER_REQUIRES_DRIVER(name__, dependency__)

#define DOOM_OS_DRIVER_REQUIRES_CAPABILITY(name__, capability__)                           \
    static constinit const ::kernel::driver::require_capability_record                     \
        DOOM_OS_DRIVER_DETAIL_CONCAT(__doom_os_driver_requires_capability_, __COUNTER__)   \
            DOOM_OS_DRIVER_DETAIL_SECTION("doom_os_driver_requires_capability") = {        \
                &DOOM_OS_DRIVER_DETAIL_ID(name__),                                         \
                &::kernel::driver::capability_key_for<capability__>}

#define DOOM_OS_DRIVER_PROVIDES(name__, capability__, api_object__) \
    DOOM_OS_DRIVER_PROVIDES_IMPL(name__, capability__, api_object__, __COUNTER__)

#define DOOM_OS_DRIVER_PROVIDES_IMPL(name__, capability__, api_object__, counter__)          \
    static const void *DOOM_OS_DRIVER_DETAIL_CONCAT(__doom_os_driver_resolve_capability_,   \
                                                    counter__)()                            \
    {                                                                                       \
        return static_cast<const void *>(&(api_object__));                                  \
    }                                                                                       \
                                                                                            \
    static constinit const ::kernel::driver::provide_record                                 \
        DOOM_OS_DRIVER_DETAIL_CONCAT(__doom_os_driver_provides_, counter__)                 \
            DOOM_OS_DRIVER_DETAIL_SECTION("doom_os_driver_provides") = {                    \
                &DOOM_OS_DRIVER_DETAIL_ID(name__),                                          \
                &::kernel::driver::capability_key_for<capability__>,                        \
                DOOM_OS_DRIVER_DETAIL_CONCAT(__doom_os_driver_resolve_capability_,          \
                                             counter__)}

#endif  // DOOM_OS_KERNEL_INCLUDE_KERNEL_DRIVER_MODULE_HPP_
