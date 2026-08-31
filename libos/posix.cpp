// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/cpu/cpu.hpp"
#include "kernel/arch/x86_64/cpu/registers.hpp"
#include "kernel/boot/boot_info.hpp"
#include "kernel/core/memory/pmm/pmm.hpp"
#include "kernel/core/memory/vmm/mmu/direct_map.hpp"
#include "kernel/debug/emit.hpp"
#include "kernel/debug/kprint.hpp"
#include "kernel/runtime/memory.hpp"

// =================================================================================================
// Newlib files
// =================================================================================================

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

// =================================================================================================
// POSIX shim
//
// Definitions for the POSIX names that have to reach the kernel, and nothing else. The split is
// not arbitrary: read/write/open/close/lseek/fstat/sbrk are the only calls in a C runtime that
// cannot be answered by computation alone. printf, malloc, memcpy and the string functions have
// nothing to forward to; newlib supplies those.
//
// Every call is defined once under newlib's underscore-prefixed porting name and exported again
// under its plain POSIX name, because which of the two libc reaches for is a property of how
// newlib was configured rather than something this file can pick. See the alias block at the
// bottom. errno belongs to newlib and is not defined here.
//
// There is no syscall here. No instruction traps, no ring changes, no register marshalling: the
// application, this file and the kernel are one binary at one privilege level, so these are calls
// the linker resolves like any other.
//
// Absent on purpose: fork, execve, wait, signal, mmap. In a unikernel they are not unimplemented,
// they are meaningless, and leaving them undefined turns a use into a link error rather than a
// runtime lie.
// =================================================================================================

namespace {

using kernel::core::paddr_t;
using kernel::core::u32;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

// =================================================================================================
// Descriptor table
//
// The one piece of state that is genuinely POSIX's rather than the kernel's. Descriptor numbers,
// the offset attached to each, and errno are concepts the kernel below deliberately does not have,
// so they stop here.
// =================================================================================================

constexpr int MAX_DESCRIPTORS = 8;
constexpr int FIRST_FILE_DESCRIPTOR = 3;

struct descriptor {
    bool      open{};
    const u8 *base{};
    u64       size{};
    u64       offset{};
};

descriptor g_descriptors[MAX_DESCRIPTORS]{};

bool is_console(int fd)
{
    return fd >= 0 && fd <= STDERR_FILENO;
}

descriptor *file_descriptor(int fd)
{
    if (fd < FIRST_FILE_DESCRIPTOR || fd >= MAX_DESCRIPTORS || !g_descriptors[fd].open) {
        return nullptr;
    }

    return &g_descriptors[fd];
}

bool same_string(const char *lhs, const char *rhs)
{
    while (*lhs != '\0' && *lhs == *rhs) {
        ++lhs;
        ++rhs;
    }

    return *lhs == *rhs;
}

// =================================================================================================
// Program break
//
// One 2 MiB page, taken on first use and never grown. sbrk's contract is a single contiguous
// region that only moves forward, which is exactly what a large page already is - and a heap that
// cannot grow is honest here, because nothing below this file can relocate one that has been
// handed out.
// =================================================================================================

u8 *g_break_base = nullptr;
u64 g_break_used = 0;

constexpr u64 BREAK_SIZE = 2 * 1024 * 1024;

bool ensure_break()
{
    namespace pmm = kernel::core::memory::pmm;
    namespace mmu = kernel::core::memory::vmm::mmu;

    if (g_break_base != nullptr) {
        return true;
    }

    const paddr_t page = pmm::alloc_page(kernel::core::memory::page_size::SIZE_2M);

    if (page == pmm::INVALID_PHYSICAL_ADDRESS) {
        return false;
    }

    g_break_base = static_cast<u8 *>(mmu::physical_window(page));

    return g_break_base != nullptr;
}

}  // namespace

// =================================================================================================
// Console
// =================================================================================================

extern "C" ssize_t _write(int fd, const void *buffer, size_t count)
{
    if (fd != STDOUT_FILENO && fd != STDERR_FILENO) {
        errno = EBADF;
        return -1;
    }

    kernel::debug::detail::emit_bytes(static_cast<const char *>(buffer), count);

    return static_cast<ssize_t>(count);
}

// =================================================================================================
// Files
//
// Boot modules are the only openable thing. They are already resident, so a read is a copy out of
// memory the bootloader placed and nothing here has to touch a device.
// =================================================================================================

extern "C" int _open(const char *path, int flags, ...)
{
    if (path == nullptr || (flags & O_WRONLY) != 0 || (flags & O_RDWR) != 0) {
        errno = EINVAL;
        return -1;
    }

    if (!kernel::boot::boot_info::available()) {
        errno = ENOENT;
        return -1;
    }

    const kernel::boot::info &boot = kernel::boot::boot_info::current();

    for (usize i = 0; i < boot.modules.count; ++i) {
        const kernel::boot::module_info &module = boot.modules[i];

        if (!same_string(path, module.command_line.c_str())) {
            continue;
        }

        void *window = kernel::core::memory::vmm::mmu::physical_window(module.range.base);

        if (window == nullptr) {
            errno = ENOMEM;
            return -1;
        }

        for (int fd = FIRST_FILE_DESCRIPTOR; fd < MAX_DESCRIPTORS; ++fd) {
            if (g_descriptors[fd].open) {
                continue;
            }

            g_descriptors[fd] = {true, static_cast<const u8 *>(window), module.range.length, 0};
            return fd;
        }

        errno = EMFILE;
        return -1;
    }

    errno = ENOENT;
    return -1;
}

extern "C" ssize_t _read(int fd, void *buffer, size_t count)
{
    // No keyboard driver yet, so stdin is at end of file rather than pretending to block.
    if (is_console(fd)) {
        return fd == STDIN_FILENO ? 0 : (errno = EBADF, -1);
    }

    descriptor *entry = file_descriptor(fd);

    if (entry == nullptr) {
        errno = EBADF;
        return -1;
    }

    const u64 remaining = entry->size - entry->offset;
    const u64 length = count < remaining ? count : remaining;

    memcpy(buffer, entry->base + entry->offset, length);
    entry->offset += length;

    return static_cast<ssize_t>(length);
}

extern "C" off_t _lseek(int fd, off_t offset, int whence)
{
    descriptor *entry = file_descriptor(fd);

    if (entry == nullptr) {
        errno = is_console(fd) ? ESPIPE : EBADF;
        return -1;
    }

    off_t target = offset;

    if (whence == SEEK_CUR) {
        target += static_cast<off_t>(entry->offset);
    } else if (whence == SEEK_END) {
        target += static_cast<off_t>(entry->size);
    } else if (whence != SEEK_SET) {
        errno = EINVAL;
        return -1;
    }

    if (target < 0 || static_cast<u64>(target) > entry->size) {
        errno = EINVAL;
        return -1;
    }

    entry->offset = static_cast<u64>(target);

    return target;
}

extern "C" int _close(int fd)
{
    descriptor *entry = file_descriptor(fd);

    if (entry == nullptr) {
        errno = EBADF;
        return -1;
    }

    *entry = {};

    return 0;
}

extern "C" int _fstat(int fd, struct stat *out)
{
    if (out == nullptr) {
        errno = EINVAL;
        return -1;
    }

    // Field by field rather than aggregate initialisation: this is newlib's struct stat, whose
    // first members are st_dev and st_ino, so a braced list would quietly fill in the wrong two.
    if (is_console(fd)) {
        out->st_mode = S_IFCHR;
        out->st_size = 0;

        return 0;
    }

    descriptor *entry = file_descriptor(fd);

    if (entry == nullptr) {
        errno = EBADF;
        return -1;
    }

    out->st_mode = S_IFREG;
    out->st_size = static_cast<off_t>(entry->size);

    return 0;
}

// =================================================================================================
// Memory
// =================================================================================================

extern "C" void *_sbrk(ptrdiff_t increment)
{
    if (!ensure_break()) {
        errno = ENOMEM;
        return reinterpret_cast<void *>(-1);
    }

    if (increment < 0 || g_break_used + static_cast<u64>(increment) > BREAK_SIZE) {
        errno = ENOMEM;
        return reinterpret_cast<void *>(-1);
    }

    u8 *previous = g_break_base + g_break_used;
    g_break_used += static_cast<u64>(increment);

    return previous;
}

// =================================================================================================
// Process
// =================================================================================================

extern "C" int _isatty(int fd)
{
    return is_console(fd) ? 1 : 0;
}

extern "C" pid_t _getpid(void)
{
    return 1;
}

// Referenced by newlib's abort() path. There is one thread of control and nothing to signal it
// with, so the only honest answer is a refusal.
extern "C" int _kill(int pid, int signal)
{
    static_cast<void>(pid);
    static_cast<void>(signal);

    errno = EINVAL;
    return -1;
}

extern "C" void _exit(int status)
{
    KPRINTLN("[app] exited with status {}", status);
    kernel::arch::x86_64::cpu::halt();
}

// =================================================================================================
// Entropy
//
// The one name in this block that is implemented rather than refused, because refusing it is
// worse than it looks. A language runtime asks for entropy in places nothing in the source
// suggests: Rust seeds every HashMap from it, so a stub here removes std::collections from the
// image for a reason no backtrace would explain. RDRAND is on every x86_64 part worth targeting
// and costs ten lines, so the honest answer is cheaper than the excuse.
//
// CPUID is checked rather than assumed - RDRAND is not architectural, and executing it without
// the bit is #UD. The retry bound is Intel's advice: the instruction reports failure through CF
// when the entropy pool is momentarily drained, and a bounded retry distinguishes that from a
// part where the DRNG is genuinely broken, which some errata describe.
// =================================================================================================

namespace {

constexpr u32 CPUID_FEATURE_LEAF = 1;
constexpr u32 CPUID_ECX_RDRAND = 1u << 30;
constexpr int RDRAND_ATTEMPTS = 10;

bool rdrand_supported()
{
    namespace cpu = kernel::arch::x86_64::cpu;

    if (cpu::cpuid(0).eax < CPUID_FEATURE_LEAF) {
        return false;
    }

    return (cpu::cpuid(CPUID_FEATURE_LEAF).ecx & CPUID_ECX_RDRAND) != 0;
}

bool rdrand64(u64 &out)
{
    for (int attempt = 0; attempt < RDRAND_ATTEMPTS; ++attempt) {
        u64           value;
        unsigned char carry;

        asm volatile("rdrand %0; setc %1" : "=r"(value), "=qm"(carry)::"cc");

        if (carry) {
            out = value;
            return true;
        }
    }

    return false;
}

}  // namespace

extern "C" ssize_t getrandom(void *buffer, size_t length, unsigned int flags)
{
    static_cast<void>(flags);

    if (buffer == nullptr) {
        errno = EFAULT;
        return -1;
    }

    if (!rdrand_supported()) {
        errno = ENOSYS;
        return -1;
    }

    u8   *out = static_cast<u8 *>(buffer);
    usize done = 0;

    while (done < length) {
        u64 word;

        if (!rdrand64(word)) {
            errno = EIO;
            return -1;
        }

        const usize chunk = (length - done) < sizeof word ? (length - done) : sizeof word;

        for (usize i = 0; i < chunk; ++i) {
            out[done + i] = static_cast<u8>(word >> (i * 8));
        }

        done += chunk;
    }

    return static_cast<ssize_t>(done);
}

// BSD's spelling of the same thing, and the one a few runtimes reach for first. All or nothing by
// contract, which getrandom already is here.
extern "C" int getentropy(void *buffer, size_t length)
{
    if (length > 256) {
        errno = EIO;
        return -1;
    }

    return getrandom(buffer, length, 0) < 0 ? -1 : 0;
}

// =================================================================================================
// Time
//
// Refused, not faked. There is no timer subsystem, so every answer this could give would be a
// lie a caller might difference against a later one and conclude that no time passes - which is
// indistinguishable from a hung clock and much harder to find than a failed call.
//
// ENOSYS rather than a panic: a runtime that asks the time is usually able to carry on without
// it, and Rust's std maps a failing clock to Unsupported rather than aborting.
// =================================================================================================

extern "C" int clock_gettime(clockid_t clock_id, struct timespec *out)
{
    static_cast<void>(clock_id);
    static_cast<void>(out);

    errno = ENOSYS;
    return -1;
}

extern "C" int _gettimeofday(struct timeval *out, void *timezone)
{
    static_cast<void>(out);
    static_cast<void>(timezone);

    errno = ENOSYS;
    return -1;
}

// There is one thread of control and nothing to wake it, so a sleep that returned would be a busy
// loop of unknown length and one that blocked would never end.
extern "C" int nanosleep(const struct timespec *requested, struct timespec *remaining)
{
    static_cast<void>(requested);
    static_cast<void>(remaining);

    errno = ENOSYS;
    return -1;
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

// =================================================================================================
// Plain POSIX names
//
// newlib's reentrant wrappers - _write_r and its siblings, which are what printf and fopen sit on
// - call the bottom of the library under one of two spellings. A newlib built the usual way calls
// _write and ships a public write() that forwards to it; a newlib built with MISSING_SYSCALL_NAMES
// has _syslist.h rewrite _write to write throughout libc and ships no wrapper at all. This
// toolchain's is the second kind, so the unprefixed names are the ones left undefined at link
// time.
//
// Aliasing rather than reimplementing keeps one body per call whichever spelling is used. The weak
// binding preserves what the prefix was for: an application defining its own write() overrides
// these rather than colliding with them.
// =================================================================================================

#define POSIX_ALIAS(target) __attribute__((weak, alias(#target)))

extern "C" int open(const char *path, int flags, ...) POSIX_ALIAS(_open);
extern "C" off_t lseek(int fd, off_t offset, int whence) POSIX_ALIAS(_lseek);
extern "C" int close(int fd) POSIX_ALIAS(_close);
extern "C" int fstat(int fd, struct stat *out) POSIX_ALIAS(_fstat);
extern "C" void *sbrk(ptrdiff_t increment) POSIX_ALIAS(_sbrk);
extern "C" int isatty(int fd) POSIX_ALIAS(_isatty);
extern "C" pid_t getpid(void) POSIX_ALIAS(_getpid);
extern "C" int kill(int pid, int signal) POSIX_ALIAS(_kill);
extern "C" int gettimeofday(struct timeval *out, void *timezone) POSIX_ALIAS(_gettimeofday);

#undef POSIX_ALIAS

// read and write are forwarded rather than aliased. newlib declares the public pair as returning
// _READ_WRITE_RETURN_TYPE, which is int on this target, while _read and _write return ssize_t: an
// alias would give one body two return types, which is a lie the pair can avoid telling for the
// price of a jump. Neither result overflows the narrower type - a count is bounded by the size_t
// the caller asked for, and the only other answer is -1.

extern "C" __attribute__((weak)) _READ_WRITE_RETURN_TYPE write(int fd, const void *buffer,
                                                               size_t count)
{
    return static_cast<_READ_WRITE_RETURN_TYPE>(_write(fd, buffer, count));
}

extern "C" __attribute__((weak)) _READ_WRITE_RETURN_TYPE read(int fd, void *buffer, size_t count)
{
    return static_cast<_READ_WRITE_RETURN_TYPE>(_read(fd, buffer, count));
}
