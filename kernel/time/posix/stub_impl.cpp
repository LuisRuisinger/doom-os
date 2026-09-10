// =================================================================================================
// POSIX ABI files
// =================================================================================================

#include "posix/abi.hpp"

namespace {

using namespace libos::abi;  // NOLINT(google-build-using-namespace) - this file *is* the ABI

}  // namespace

// =================================================================================================
// Time
//
// Refused, not faked. There is no timer subsystem, so every answer this could give would be a lie
// a caller might difference against a later one and conclude that no time passes.
// =================================================================================================

extern "C" int clock_gettime(clockid_t clock_id, timespec *out)
{
    static_cast<void>(clock_id);
    static_cast<void>(out);

    errno = ENOSYS;
    return -1;
}

extern "C" int gettimeofday(timeval *out, void *timezone)
{
    static_cast<void>(out);
    static_cast<void>(timezone);

    errno = ENOSYS;
    return -1;
}

// There is one thread of control and nothing to wake it, so a sleep that returned would be a busy
// loop of unknown length and one that blocked would never end.
extern "C" int nanosleep(const timespec *requested, timespec *remaining)
{
    static_cast<void>(requested);
    static_cast<void>(remaining);

    errno = ENOSYS;
    return -1;
}
