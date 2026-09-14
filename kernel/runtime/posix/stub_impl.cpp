
#include "arch/x86_64/cpu/cpu.hpp"
#include "kernel/debug/kprint.hpp"
#include "posix/abi.hpp"

namespace {

using namespace libos::abi;  // NOLINT(google-build-using-namespace) - this file *is* the ABI

bool is_console(int fd)
{
    return fd >= 0 && fd <= STDERR_FILENO;
}

}  // namespace

extern "C" int isatty(int fd)
{
    return is_console(fd) ? 1 : 0;
}

extern "C" pid_t getpid(void)
{
    return 1;
}

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

namespace {

char *m_empty_environment[] = {nullptr};

}  // namespace

extern "C" {

char **environ = m_empty_environment;
}
