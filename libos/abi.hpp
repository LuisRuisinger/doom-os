#ifndef DOOM_OS_LIBOS_ABI_HPP_
#define DOOM_OS_LIBOS_ABI_HPP_

// =================================================================================================
// The ABI this image implements
//
// x86_64 Linux, as glibc presents it. Every type, constant and layout here is fixed by that ABI
// rather than chosen, so the numbers are copied rather than invented and none of them may be
// changed to something more convenient.
//
// It is written out instead of included because there is no C library in this repository to
// include it from. Which libc an image carries is the integrator's decision, exactly as the
// application and its drivers are, so this file is the contract both sides compile against and
// neither side ships.
//
// The risk of restating a declaration is real - a prototype that drifts from the caller's is a
// bug no compiler here can see - which is why this is one header rather than a declaration at
// each use, and why the struct layouts carry their offsets.
// =================================================================================================

namespace libos::abi {

// =================================================================================================
// Types
// =================================================================================================

using size_t = unsigned long;
using ssize_t = long;
using off_t = long;
using pid_t = int;
using mode_t = unsigned int;
using clockid_t = int;
using ptrdiff_t = long;

// =================================================================================================
// errno
//
// Linux's numbers, which are not the same as any other Unix's. ENOSYS is 38 here; a libc from a
// different family would call that ENOTNAM and report something unrelated.
// =================================================================================================

inline constexpr int EPERM = 1;
inline constexpr int ENOENT = 2;
inline constexpr int EIO = 5;
inline constexpr int EBADF = 9;
inline constexpr int ENOMEM = 12;
inline constexpr int EFAULT = 14;
inline constexpr int EINVAL = 22;
inline constexpr int EMFILE = 24;
inline constexpr int ESPIPE = 29;
inline constexpr int ENOSYS = 38;

// =================================================================================================
// Descriptors and files
// =================================================================================================

inline constexpr int STDIN_FILENO = 0;
inline constexpr int STDOUT_FILENO = 1;
inline constexpr int STDERR_FILENO = 2;

inline constexpr int O_RDONLY = 0;
inline constexpr int O_WRONLY = 1;
inline constexpr int O_RDWR = 2;

inline constexpr int SEEK_SET = 0;
inline constexpr int SEEK_CUR = 1;
inline constexpr int SEEK_END = 2;

inline constexpr mode_t S_IFCHR = 0020000;
inline constexpr mode_t S_IFREG = 0100000;

// =================================================================================================
// Structures
//
// Offsets are asserted rather than trusted. A caller compiled against glibc's headers reads these
// at fixed displacements, so a field in the wrong place is not a compile error anywhere - it is a
// value silently taken from the middle of another one.
// =================================================================================================

struct timespec {
    long tv_sec;
    long tv_nsec;
};

struct timeval {
    long tv_sec;
    long tv_usec;
};

struct stat {
    unsigned long dev;
    unsigned long ino;
    unsigned long nlink;
    mode_t        mode;
    unsigned int  uid;
    unsigned int  gid;
    int           pad0;
    unsigned long rdev;
    off_t         size;
    long          blksize;
    long          blocks;
    timespec      atim;
    timespec      mtim;
    timespec      ctim;
    long          reserved[3];
};

static_assert(sizeof(stat) == 144, "x86_64 Linux struct stat is 144 bytes");
static_assert(__builtin_offsetof(stat, mode) == 24, "st_mode sits at 24");
static_assert(__builtin_offsetof(stat, size) == 48, "st_size sits at 48");
static_assert(__builtin_offsetof(stat, atim) == 72, "st_atim sits at 72");
static_assert(sizeof(timespec) == 16, "timespec is two longs");

}  // namespace libos::abi

// =================================================================================================
// errno storage
//
// One int, because there is one thread. glibc reaches it through __errno_location rather than
// directly, which is what makes it replaceable later without touching a caller.
// =================================================================================================

extern "C" int errno;

#endif  // DOOM_OS_LIBOS_ABI_HPP_
