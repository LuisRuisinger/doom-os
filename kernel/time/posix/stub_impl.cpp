
#include "posix/abi.hpp"

namespace {

using namespace libos::abi;  // NOLINT(google-build-using-namespace) - this file *is* the ABI

}  // namespace

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

extern "C" int nanosleep(const timespec *requested, timespec *remaining)
{
    static_cast<void>(requested);
    static_cast<void>(remaining);

    errno = ENOSYS;
    return -1;
}
