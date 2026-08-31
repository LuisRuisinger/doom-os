// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/debug/kprint.hpp"

// =================================================================================================
// Newlib files
// =================================================================================================

#include <errno.h>

// =================================================================================================
// glibc ABI
//
// The surface a binary built for x86_64-unknown-linux-gnu resolves against, so that unmodified
// Linux objects link into this image. It sits beside libos/posix.cpp rather than replacing it:
// the two do not collide, because everything here is either a name newlib does not have at all
// (__errno_location, pthread_*, getauxval) or glibc's large-file spelling of one it does
// (open64 against open, fstat64 against fstat). The shared names - write, malloc, memcpy - are
// left to newlib, whose behaviour for those already agrees with glibc's.
//
// Almost all of it is unimplemented, and that is the intended state rather than a gap to
// apologise for. The point of a stub surface is that a Linux binary links, runs, and tells you
// which call it actually needed - which is a far cheaper way to find the real requirement than
// reading glibc's headers and guessing. Each stub reports itself once and returns the failure
// its caller is required to handle.
//
// Stubs are declared variadic. Nothing here reads its arguments, and a variadic definition cannot
// disagree with the prototype the caller was compiled against - which matters when the caller's
// header is glibc's and this file has never seen it. The SysV register conventions make the call
// itself identical either way, and the return value lands in RAX regardless.
//
// What is missing before any of this runs is not on this list: thread-local storage. The build
// that produced these names also carries 35 TLSGD and 16 TLSLD relocations and calls
// __tls_get_addr, so %fs has to point at a laid-out TLS block with glibc's tcbhead_t before the
// first std function executes. That is kernel work, and it is the real prerequisite.
// =================================================================================================

namespace {

using kernel::core::i32;
using kernel::core::u64;
using kernel::core::usize;

// One line per distinct call, not per invocation: a stub reached inside a loop should not bury
// the report of the next one.
bool report_once(const char *name, bool &reported)
{
    if (!reported) {
        reported = true;
        KPRINTLN("[glibc] unimplemented: {}", name);
    }

    return true;
}

}  // namespace

#define DOOM_OS_GLIBC_STUB(name, result)                     \
    extern "C" long name(...)                                \
    {                                                        \
        static bool reported_m = false;                      \
        report_once(#name, reported_m);                      \
        errno = ENOSYS;                                       \
        return (result);                                     \
    }

// =================================================================================================
// Process and signals
// =================================================================================================

DOOM_OS_GLIBC_STUB(fork, -1)
DOOM_OS_GLIBC_STUB(execvp, -1)
DOOM_OS_GLIBC_STUB(waitpid, -1)
DOOM_OS_GLIBC_STUB(waitid, -1)
DOOM_OS_GLIBC_STUB(pause, -1)
DOOM_OS_GLIBC_STUB(getppid, -1)
DOOM_OS_GLIBC_STUB(getuid, -1)
DOOM_OS_GLIBC_STUB(setuid, -1)
DOOM_OS_GLIBC_STUB(setgid, -1)
DOOM_OS_GLIBC_STUB(setgroups, -1)
DOOM_OS_GLIBC_STUB(setpgid, -1)
DOOM_OS_GLIBC_STUB(setsid, -1)
DOOM_OS_GLIBC_STUB(sched_yield, -1)
DOOM_OS_GLIBC_STUB(sched_getaffinity, -1)
DOOM_OS_GLIBC_STUB(signal, -1)
DOOM_OS_GLIBC_STUB(sigaction, -1)
DOOM_OS_GLIBC_STUB(sigaltstack, -1)
DOOM_OS_GLIBC_STUB(sigaddset, -1)
DOOM_OS_GLIBC_STUB(sigemptyset, -1)
DOOM_OS_GLIBC_STUB(getpwuid_r, -1)

// =================================================================================================
// posix_spawn
// =================================================================================================

DOOM_OS_GLIBC_STUB(posix_spawnattr_destroy, 0)
DOOM_OS_GLIBC_STUB(posix_spawnattr_init, 0)
DOOM_OS_GLIBC_STUB(posix_spawnattr_setflags, 0)
DOOM_OS_GLIBC_STUB(posix_spawnattr_setpgroup, 0)
DOOM_OS_GLIBC_STUB(posix_spawnattr_setsigdefault, 0)
DOOM_OS_GLIBC_STUB(posix_spawn_file_actions_adddup2, 0)
DOOM_OS_GLIBC_STUB(posix_spawn_file_actions_destroy, 0)
DOOM_OS_GLIBC_STUB(posix_spawn_file_actions_init, 0)
DOOM_OS_GLIBC_STUB(posix_spawnp, 0)

// =================================================================================================
// Threads
// =================================================================================================

DOOM_OS_GLIBC_STUB(pthread_attr_destroy, 0)
DOOM_OS_GLIBC_STUB(pthread_attr_getguardsize, 0)
DOOM_OS_GLIBC_STUB(pthread_attr_getstack, 0)
DOOM_OS_GLIBC_STUB(pthread_attr_init, 0)
DOOM_OS_GLIBC_STUB(pthread_attr_setstacksize, 0)
DOOM_OS_GLIBC_STUB(pthread_create, -1)
DOOM_OS_GLIBC_STUB(pthread_detach, -1)
DOOM_OS_GLIBC_STUB(pthread_getattr_np, -1)
DOOM_OS_GLIBC_STUB(pthread_join, -1)
DOOM_OS_GLIBC_STUB(pthread_key_create, -1)
DOOM_OS_GLIBC_STUB(pthread_key_delete, -1)
DOOM_OS_GLIBC_STUB(pthread_self, -1)
DOOM_OS_GLIBC_STUB(pthread_setname_np, -1)
DOOM_OS_GLIBC_STUB(pthread_setspecific, -1)

// =================================================================================================
// Memory mapping
// =================================================================================================

DOOM_OS_GLIBC_STUB(mmap64, -1)
DOOM_OS_GLIBC_STUB(munmap, -1)
DOOM_OS_GLIBC_STUB(mprotect, -1)

// =================================================================================================
// Files and descriptors
// =================================================================================================

DOOM_OS_GLIBC_STUB(open64, -1)
DOOM_OS_GLIBC_STUB(openat64, -1)
DOOM_OS_GLIBC_STUB(lseek64, -1)
DOOM_OS_GLIBC_STUB(stat64, -1)
DOOM_OS_GLIBC_STUB(fstat64, -1)
DOOM_OS_GLIBC_STUB(fstatat64, -1)
DOOM_OS_GLIBC_STUB(lstat64, -1)
DOOM_OS_GLIBC_STUB(ftruncate64, -1)
DOOM_OS_GLIBC_STUB(pread64, -1)
DOOM_OS_GLIBC_STUB(pwrite64, -1)
DOOM_OS_GLIBC_STUB(preadv, -1)
DOOM_OS_GLIBC_STUB(pwritev, -1)
DOOM_OS_GLIBC_STUB(readv, -1)
DOOM_OS_GLIBC_STUB(writev, -1)
DOOM_OS_GLIBC_STUB(dup, -1)
DOOM_OS_GLIBC_STUB(dup2, -1)
DOOM_OS_GLIBC_STUB(pipe2, -1)
DOOM_OS_GLIBC_STUB(poll, -1)
DOOM_OS_GLIBC_STUB(ioctl, -1)
DOOM_OS_GLIBC_STUB(flock, -1)
DOOM_OS_GLIBC_STUB(fsync, -1)
DOOM_OS_GLIBC_STUB(fdatasync, -1)
DOOM_OS_GLIBC_STUB(sendfile64, -1)
DOOM_OS_GLIBC_STUB(splice, -1)
DOOM_OS_GLIBC_STUB(readlink, -1)
DOOM_OS_GLIBC_STUB(rename, -1)
DOOM_OS_GLIBC_STUB(unlink, -1)
DOOM_OS_GLIBC_STUB(unlinkat, -1)
DOOM_OS_GLIBC_STUB(linkat, -1)
DOOM_OS_GLIBC_STUB(symlink, -1)
DOOM_OS_GLIBC_STUB(mkdir, -1)
DOOM_OS_GLIBC_STUB(rmdir, -1)
DOOM_OS_GLIBC_STUB(mkfifo, -1)
DOOM_OS_GLIBC_STUB(chdir, -1)
DOOM_OS_GLIBC_STUB(chmod, -1)
DOOM_OS_GLIBC_STUB(fchmod, -1)
DOOM_OS_GLIBC_STUB(chown, -1)
DOOM_OS_GLIBC_STUB(fchown, -1)
DOOM_OS_GLIBC_STUB(lchown, -1)
DOOM_OS_GLIBC_STUB(chroot, -1)
DOOM_OS_GLIBC_STUB(getcwd, -1)
DOOM_OS_GLIBC_STUB(utimensat, -1)
DOOM_OS_GLIBC_STUB(futimens, -1)

// =================================================================================================
// Directories
// =================================================================================================

DOOM_OS_GLIBC_STUB(opendir, -1)
DOOM_OS_GLIBC_STUB(fdopendir, -1)
DOOM_OS_GLIBC_STUB(closedir, 0)
DOOM_OS_GLIBC_STUB(readdir64, -1)
DOOM_OS_GLIBC_STUB(dirfd, -1)

// =================================================================================================
// Sockets and resolver
// =================================================================================================

DOOM_OS_GLIBC_STUB(socket, -1)
DOOM_OS_GLIBC_STUB(socketpair, -1)
DOOM_OS_GLIBC_STUB(bind, -1)
DOOM_OS_GLIBC_STUB(listen, -1)
DOOM_OS_GLIBC_STUB(connect, -1)
DOOM_OS_GLIBC_STUB(accept4, -1)
DOOM_OS_GLIBC_STUB(send, -1)
DOOM_OS_GLIBC_STUB(sendto, -1)
DOOM_OS_GLIBC_STUB(sendmsg, -1)
DOOM_OS_GLIBC_STUB(recv, -1)
DOOM_OS_GLIBC_STUB(recvfrom, -1)
DOOM_OS_GLIBC_STUB(recvmsg, -1)
DOOM_OS_GLIBC_STUB(shutdown, -1)
DOOM_OS_GLIBC_STUB(getsockname, -1)
DOOM_OS_GLIBC_STUB(getpeername, -1)
DOOM_OS_GLIBC_STUB(getsockopt, -1)
DOOM_OS_GLIBC_STUB(setsockopt, -1)
DOOM_OS_GLIBC_STUB(getaddrinfo, -1)
DOOM_OS_GLIBC_STUB(freeaddrinfo, 0)
DOOM_OS_GLIBC_STUB(gai_strerror, -1)
DOOM_OS_GLIBC_STUB(gethostname, -1)
DOOM_OS_GLIBC_STUB(__res_init, -1)

// =================================================================================================
// Dynamic linking and unwinding
// =================================================================================================

DOOM_OS_GLIBC_STUB(dlsym, -1)
DOOM_OS_GLIBC_STUB(dl_iterate_phdr, -1)
DOOM_OS_GLIBC_STUB(_Unwind_Backtrace, -1)
DOOM_OS_GLIBC_STUB(_Unwind_FindEnclosingFunction, -1)
DOOM_OS_GLIBC_STUB(_Unwind_GetCFA, -1)
DOOM_OS_GLIBC_STUB(_Unwind_GetDataRelBase, -1)
DOOM_OS_GLIBC_STUB(_Unwind_GetIP, -1)
DOOM_OS_GLIBC_STUB(_Unwind_GetIPInfo, -1)
DOOM_OS_GLIBC_STUB(_Unwind_GetLanguageSpecificData, -1)
DOOM_OS_GLIBC_STUB(_Unwind_GetRegionStart, -1)
DOOM_OS_GLIBC_STUB(_Unwind_GetTextRelBase, -1)
DOOM_OS_GLIBC_STUB(_Unwind_Resume, -1)
DOOM_OS_GLIBC_STUB(_Unwind_SetGR, 0)
DOOM_OS_GLIBC_STUB(_Unwind_SetIP, 0)

// =================================================================================================
// Time
// =================================================================================================

DOOM_OS_GLIBC_STUB(clock_nanosleep, -1)

// =================================================================================================
// Names newlib does not have
//
// fcntl is newlib's, but its body calls a _fcntl the porting layer does not define, so defining
// it here refuses at the call rather than failing the link. __xpg_strerror_r is glibc's spelling
// of strerror_r and has no newlib equivalent at all.
// =================================================================================================

DOOM_OS_GLIBC_STUB(fcntl, -1)
DOOM_OS_GLIBC_STUB(__xpg_strerror_r, -1)

#undef DOOM_OS_GLIBC_STUB

// =================================================================================================
// Implemented
//
// The handful that cannot be a stub because their callers do not check them, or because a wrong
// answer is worse than a refusal.
// =================================================================================================

// glibc reaches errno through a function so that it can be per-thread. There is one thread here,
// so it is newlib's single errno - which is the same object every other file in this image means.
extern "C" int *__errno_location()
{
    return &errno;
}

// No auxiliary vector: nothing loads an ELF here, the kernel calls main directly. Zero is the
// documented answer for an absent entry, and callers treat it as "not provided" rather than as a
// value, which is exactly right.
extern "C" unsigned long getauxval(unsigned long type)
{
    static_cast<void>(type);

    return 0;
}

extern "C" const char *gnu_get_libc_version()
{
    return "2.39";
}

extern "C" int posix_memalign(void **out, kernel::core::usize alignment, kernel::core::usize size)
{
    static_cast<void>(alignment);
    static_cast<void>(size);

    if (out != nullptr) {
        *out = nullptr;
    }

    return ENOMEM;
}

extern "C" long sysconf(int name)
{
    // _SC_PAGESIZE is the one a runtime asks before it can lay anything out, and answering it
    // wrongly is not recoverable, so it is answered rather than refused.
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

// Reached only from code that already decided to die, so there is nothing to return to.
extern "C" [[noreturn]] void __compilerrt_abort_impl(const char *file, int line, const char *func)
{
    KPANIC("compiler-rt abort in {} at {}:{}", func, file, line);
}

// The generic syscall entry glibc exposes for calls it has no wrapper for. Rust's std reaches it
// for getrandom and futex. Dispatching it properly is the shape of the eventual Linux ABI layer;
// until then it refuses, which is distinguishable from a syscall that silently did nothing.
extern "C" long syscall(long number, ...)
{
    KPRINTLN("[glibc] unimplemented syscall {}", number);

    errno = ENOSYS;
    return -1;
}

// General-dynamic TLS. Reached before main on any std binary, and there is nothing sensible to
// return, so it stops here rather than handing back a pointer into nothing.
extern "C" [[noreturn]] void *__tls_get_addr(void *descriptor)
{
    static_cast<void>(descriptor);

    KPANIC("__tls_get_addr: thread-local storage is not set up yet");
}

// Rust names this in .eh_frame even under panic=abort. Nothing unwinds here.
extern "C" [[noreturn]] int rust_eh_personality()
{
    KPANIC("rust_eh_personality: nothing unwinds in this image");
}
