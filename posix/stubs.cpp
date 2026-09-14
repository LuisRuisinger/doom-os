
#include <type_traits>

#include "kernel/debug/kpanic.hpp"
#include "kernel/debug/kprint.hpp"
#include "posix/abi.hpp"

namespace {

using namespace libos::abi;  // NOLINT(google-build-using-namespace) - this file *is* the ABI

bool report_once(const char *name, bool &reported)
{
    if (!reported) {
        reported = true;
        KPRINTLN("[posix] fallback: {}", name);
    }

    return true;
}

template <typename Return, typename MakeValue>
Return finish_stub(MakeValue make_value)
{
    if constexpr (std::is_void_v<Return>) {
        make_value();
        return;
    } else {
        return make_value();
    }
}

}  // namespace

#define DOOM_OS_WEAK __attribute__((weak))

#define POSIX_STUB(return_type__, name__, value__, errno__, ...)     \
    extern "C" DOOM_OS_WEAK return_type__ name__(__VA_ARGS__)        \
    {                                                                \
        static bool reported_m = false;                              \
        report_once(#name__, reported_m);                            \
        if constexpr ((errno__) != 0) {                              \
            errno = (errno__);                                       \
        }                                                            \
        return finish_stub<return_type__>([] { return (value__); }); \
    }

#include "posix/symbols.def"

#undef POSIX_STUB

extern "C" [[noreturn]] void _exit(int status);

extern "C" void *mmap(void *address, size_t length, int protection, int flags, int fd,
                      off_t offset);
extern "C" void *mremap(void *old_address, size_t old_size, size_t new_size, int flags, ...);

extern "C" [[noreturn]] DOOM_OS_WEAK void _Exit(int status)
{
    _exit(status);
}

extern "C" [[noreturn]] DOOM_OS_WEAK void exit(int status)
{
    _exit(status);
}

extern "C" [[noreturn]] DOOM_OS_WEAK void abort()
{
    KPANIC("abort() called");
}

extern "C" DOOM_OS_WEAK int *__errno_location()
{
    return &errno;
}

extern "C" DOOM_OS_WEAK int *___errno_location()
{
    return &errno;
}

extern "C" DOOM_OS_WEAK unsigned long getauxval(unsigned long type)
{
    static_cast<void>(type);

    return 0;
}

extern "C" DOOM_OS_WEAK unsigned long __getauxval(unsigned long type)
{
    return getauxval(type);
}

extern "C" DOOM_OS_WEAK void *__mmap(void *address, size_t length, int protection, int flags,
                                     int fd, off_t offset)
{
    return mmap(address, length, protection, flags, fd, offset);
}

extern "C" DOOM_OS_WEAK void *__mremap(void *old_address, size_t old_size, size_t new_size,
                                       int flags, ...)
{
    return mremap(old_address, old_size, new_size, flags);
}

extern "C" DOOM_OS_WEAK long sysconf(int name)
{
    constexpr int SC_PAGESIZE = 30;
    constexpr int SC_NPROCESSORS_ONLN = 84;

    if (name == SC_PAGESIZE) {
        return 4096;
    }

    if (name == SC_NPROCESSORS_ONLN) {
        return 1;
    }

    errno = EINVAL;
    return -1;
}

extern "C" DOOM_OS_WEAK long syscall(long number, ...)
{
    KPRINTLN("[musl] unsupported syscall {}", number);

    errno = ENOSYS;
    return -1;
}

extern "C" [[noreturn]] DOOM_OS_WEAK void *__tls_get_addr(void *descriptor)
{
    static_cast<void>(descriptor);

    KPANIC("__tls_get_addr: thread-local storage is not set up yet");
}

extern "C" [[noreturn]] DOOM_OS_WEAK void __compilerrt_abort_impl(const char *file, int line,
                                                                  const char *func)
{
    KPANIC("compiler-rt abort in {} at {}:{}", func, file, line);
}

extern "C" [[noreturn]] DOOM_OS_WEAK int rust_eh_personality()
{
    KPANIC("rust_eh_personality: nothing unwinds in this image");
}

#undef DOOM_OS_WEAK
