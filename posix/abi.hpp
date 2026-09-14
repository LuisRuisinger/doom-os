#ifndef DOOM_OS_POSIX_ABI_HPP_
#define DOOM_OS_POSIX_ABI_HPP_

namespace libos::abi {

using size_t = unsigned long;
using ssize_t = long;
using off_t = long;
using pid_t = int;
using uid_t = unsigned int;
using gid_t = unsigned int;
using mode_t = unsigned int;
using clockid_t = int;
using ptrdiff_t = long;
using id_t = unsigned int;
using idtype_t = int;
using nfds_t = unsigned long;
using socklen_t = unsigned int;
using pthread_t = unsigned long;
using pthread_key_t = unsigned int;

inline constexpr size_t SSIZE_MAX_VALUE = (size_t{1} << (sizeof(ssize_t) * 8 - 1)) - 1;

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

inline constexpr int IOV_MAX = 1024;

inline constexpr int PROT_NONE = 0x0;
inline constexpr int PROT_READ = 0x1;
inline constexpr int PROT_WRITE = 0x2;
inline constexpr int PROT_EXEC = 0x4;

inline constexpr int MAP_PRIVATE = 0x02;
inline constexpr int MAP_ANONYMOUS = 0x20;
inline constexpr int MAP_NORESERVE = 0x4000;

struct timespec {
    long tv_sec;
    long tv_nsec;
};

struct timeval {
    long tv_sec;
    long tv_usec;
};

struct timezone;

struct iovec {
    void  *iov_base;
    size_t iov_len;
};

struct addrinfo;
struct DIR;
struct dirent;
struct dl_phdr_info;
struct msghdr;
struct pollfd;
struct pthread_attr_t;
struct siginfo_t;
struct sockaddr;

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
static_assert(sizeof(iovec) == 16, "iovec is two machine words");

}  // namespace libos::abi

extern "C" int errno;

#endif  // DOOM_OS_POSIX_ABI_HPP_
