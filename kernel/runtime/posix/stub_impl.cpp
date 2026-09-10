// =================================================================================================
// POSIX ABI files
// =================================================================================================

#include "posix/abi.hpp"

// =================================================================================================
// Architecture files
// =================================================================================================

#include "arch/x86_64/cpu/cpu.hpp"

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/debug/kprint.hpp"

namespace {

using namespace libos::abi;  // NOLINT(google-build-using-namespace) - this file *is* the ABI

bool is_console(int fd)
{
    return fd >= 0 && fd <= STDERR_FILENO;
}

}  // namespace

// =================================================================================================
// Process
// =================================================================================================

extern "C" int isatty(int fd)
{
    return is_console(fd) ? 1 : 0;
}

extern "C" pid_t getpid(void)
{
    return 1;
}

// Referenced by any libc's abort() path. There is one thread of control and nothing to signal it
// with, so the only honest answer is a refusal.
extern "C" int kill(int pid, int signal)
{
    static_cast<void>(pid);
    static_cast<void>(signal);

    errno = EINVAL;
    return -1;
}

extern "C" [[noreturn]] void _exit(int status)
{
    KPRINTLN("[app] exited with status {}", status);
    kernel::arch::x86_64::cpu::halt();
}

// =================================================================================================
// Environment
//
// Empty rather than absent. getenv walks this array and has no way to ask whether an environment
// exists, so the terminator is the whole contract: every lookup misses, nothing faults.
// =================================================================================================

namespace {

char *g_empty_environment[] = {nullptr};

}  // namespace

extern "C" {
char **environ = g_empty_environment;
}
