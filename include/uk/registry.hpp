#ifndef DOOM_OS_INCLUDE_UK_REGISTRY_HPP_
#define DOOM_OS_INCLUDE_UK_REGISTRY_HPP_

// =================================================================================================
// Kernel public files
// =================================================================================================

#include <uk/types.hpp>

namespace uk::detail {

[[noreturn]] void result_error(const char *message);

}  // namespace uk::detail

#ifndef RESULT_ERROR
#    define RESULT_ERROR(message__) ::uk::detail::result_error(message__)
#endif

// =================================================================================================
// Third-party files
// =================================================================================================

#include <result/result.hpp>

namespace uk {

using lsr::result::Err;
using lsr::result::Ok;
using lsr::result::Result;

// =================================================================================================
// Driver records
// =================================================================================================

enum class capability_multiplicity : u8 {
    SINGLE,
    MULTIPLE,
};

struct driver_id {
    const char *name;
};

struct capability_key {
    const char              *name;
    capability_multiplicity  multiplicity;
};

enum class error : u8 {
    UNSPECIFIED,
    CAPACITY_EXCEEDED,
    INVALID_RECORD,
    DUPLICATE_DRIVER,
    DUPLICATE_DRIVER_ENTRY,
    DUPLICATE_DRIVER_EXIT,
    UNKNOWN_DRIVER,
    UNKNOWN_DRIVER_DEPENDENCY,
    MISSING_CAPABILITY_PROVIDER,
    MULTIPLE_CAPABILITY_PROVIDERS,
    DEPENDENCY_CYCLE,
    CAPABILITY_RESOLVE_FAILED,
};

using init_result = Result<void, error>;
using init_fn = init_result (*)();
using exit_fn = void (*)();
using capability_resolve_fn = const void *(*)();

struct init_record {
    const driver_id *driver;
    init_fn          init;
};

struct exit_record {
    const driver_id *driver;
    exit_fn          exit;
};

struct provide_record {
    const driver_id       *driver;
    const capability_key  *capability;
    capability_resolve_fn  resolve;
};

struct require_driver_record {
    const driver_id *driver;
    const char      *dependency_name;
};

struct require_capability_record {
    const driver_id      *driver;
    const capability_key *capability;
};

const char *describe(error value);

// =================================================================================================
// Linker section ranges
// =================================================================================================

template <typename T>
class section_range {
    const T *begin_m;
    const T *end_m;

public:
    constexpr section_range(const T *begin, const T *end)
        : begin_m(begin)
        , end_m(end)
    {
    }

    [[nodiscard]] const T *begin() const
    {
        return begin_m == nullptr || end_m == nullptr ? nullptr : begin_m;
    }

    [[nodiscard]] const T *end() const
    {
        return begin_m == nullptr || end_m == nullptr ? nullptr : end_m;
    }

    [[nodiscard]] bool empty() const
    {
        return begin() == end();
    }

    [[nodiscard]] usize size() const
    {
        const T *first = begin();
        const T *last = end();

        return first == nullptr || last == nullptr ? usize{0} : static_cast<usize>(last - first);
    }
};

}  // namespace uk

extern "C" {

extern const uk::driver_id __start_doom_os_driver_ids[]
    __attribute__((weak));
extern const uk::driver_id __stop_doom_os_driver_ids[]
    __attribute__((weak));

extern const uk::init_record __start_doom_os_driver_init[]
    __attribute__((weak));
extern const uk::init_record __stop_doom_os_driver_init[]
    __attribute__((weak));

extern const uk::exit_record __start_doom_os_driver_exit[]
    __attribute__((weak));
extern const uk::exit_record __stop_doom_os_driver_exit[]
    __attribute__((weak));

extern const uk::provide_record __start_doom_os_driver_provides[]
    __attribute__((weak));
extern const uk::provide_record __stop_doom_os_driver_provides[]
    __attribute__((weak));

extern const uk::require_driver_record __start_doom_os_driver_requires_driver[]
    __attribute__((weak));
extern const uk::require_driver_record __stop_doom_os_driver_requires_driver[]
    __attribute__((weak));

extern const uk::require_capability_record
    __start_doom_os_driver_requires_capability[] __attribute__((weak));
extern const uk::require_capability_record
    __stop_doom_os_driver_requires_capability[] __attribute__((weak));
}

namespace uk::registry {

inline section_range<driver_id> drivers()
{
    return section_range<driver_id>{__start_doom_os_driver_ids, __stop_doom_os_driver_ids};
}

inline section_range<init_record> init_records()
{
    return section_range<init_record>{__start_doom_os_driver_init, __stop_doom_os_driver_init};
}

inline section_range<exit_record> exit_records()
{
    return section_range<exit_record>{__start_doom_os_driver_exit, __stop_doom_os_driver_exit};
}

inline section_range<provide_record> provide_records()
{
    return section_range<provide_record>{__start_doom_os_driver_provides,
                                         __stop_doom_os_driver_provides};
}

inline section_range<require_driver_record> require_driver_records()
{
    return section_range<require_driver_record>{__start_doom_os_driver_requires_driver,
                                                __stop_doom_os_driver_requires_driver};
}

inline section_range<require_capability_record> require_capability_records()
{
    return section_range<require_capability_record>{
        __start_doom_os_driver_requires_capability,
        __stop_doom_os_driver_requires_capability};
}

}  // namespace uk::registry

#endif  // DOOM_OS_INCLUDE_UK_REGISTRY_HPP_
