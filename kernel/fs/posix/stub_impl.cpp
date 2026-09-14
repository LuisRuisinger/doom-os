
#include "kernel/boot/boot_info.hpp"
#include "kernel/core/cast.hpp"
#include "kernel/debug/emit.hpp"
#include "kernel/mm/vmm.hpp"
#include "posix/abi.hpp"

namespace {

using namespace libos::abi;  // NOLINT(google-build-using-namespace) - this file *is* the ABI

using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

constexpr int MAX_DESCRIPTORS = 8;
constexpr int FIRST_FILE_DESCRIPTOR = 3;

struct descriptor {
    bool      open{};
    const u8 *base{};
    u64       size{};
    u64       offset{};
};

descriptor m_descriptors[MAX_DESCRIPTORS]{};

bool is_console(int fd)
{
    return fd >= 0 && fd <= STDERR_FILENO;
}

descriptor *file_descriptor(int fd)
{
    if (fd < FIRST_FILE_DESCRIPTOR || fd >= MAX_DESCRIPTORS || !m_descriptors[fd].open) {
        return nullptr;
    }

    return &m_descriptors[fd];
}

bool same_string(const char *lhs, const char *rhs)
{
    while (*lhs != '\0' && *lhs == *rhs) {
        ++lhs;
        ++rhs;
    }

    return *lhs == *rhs;
}

void copy_bytes(void *destination, const void *source, u64 length)
{
    auto       *out = destination as(u8 *);
    const auto *in = source as(const u8 *);

    for (u64 i = 0; i < length; ++i) {
        out[i] = in[i];
    }
}

bool validate_iovecs(const iovec *vectors, int count)
{
    if (count < 0 || count > IOV_MAX || (vectors == nullptr && count != 0)) {
        errno = EINVAL;
        return false;
    }

    size_t total = 0;

    for (int i = 0; i < count; ++i) {
        if (vectors[i].iov_base == nullptr && vectors[i].iov_len != 0) {
            errno = EFAULT;
            return false;
        }

        if (vectors[i].iov_len > SSIZE_MAX_VALUE - total) {
            errno = EINVAL;
            return false;
        }

        total += vectors[i].iov_len;
    }

    return true;
}

}  // namespace

extern "C" ssize_t write(int fd, const void *buffer, size_t count)
{
    if (fd != STDOUT_FILENO && fd != STDERR_FILENO) {
        errno = EBADF;
        return -1;
    }

    kernel::debug::detail::emit_bytes(buffer as(const char *), count);

    return count as(ssize_t);
}

extern "C" ssize_t writev(int fd, const iovec *vectors, int count)
{
    if (!validate_iovecs(vectors, count)) {
        return -1;
    }

    ssize_t total = 0;

    for (int i = 0; i < count; ++i) {
        const ssize_t written = write(fd, vectors[i].iov_base, vectors[i].iov_len);

        if (written < 0) {
            return total == 0 ? -1 : total;
        }

        total += written;

        if (written as(size_t) != vectors[i].iov_len) {
            break;
        }
    }

    return total;
}

extern "C" int open(const char *path, int flags, ...)
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

        void *window = kernel::mm::vmm::phy_to_vrt(module.range.base);

        if (window == nullptr) {
            errno = ENOMEM;
            return -1;
        }

        for (int fd = FIRST_FILE_DESCRIPTOR; fd < MAX_DESCRIPTORS; ++fd) {
            if (m_descriptors[fd].open) {
                continue;
            }

            m_descriptors[fd] = {true, window as(const u8 *), module.range.length, 0};
            return fd;
        }

        errno = EMFILE;
        return -1;
    }

    errno = ENOENT;
    return -1;
}

extern "C" ssize_t read(int fd, void *buffer, size_t count)
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

    copy_bytes(buffer, entry->base + entry->offset, length);
    entry->offset += length;

    return length as(ssize_t);
}

extern "C" ssize_t readv(int fd, const iovec *vectors, int count)
{
    if (!validate_iovecs(vectors, count)) {
        return -1;
    }

    ssize_t total = 0;

    for (int i = 0; i < count; ++i) {
        const ssize_t bytes_read = read(fd, vectors[i].iov_base, vectors[i].iov_len);

        if (bytes_read < 0) {
            return total == 0 ? -1 : total;
        }

        total += bytes_read;

        if (bytes_read as(size_t) != vectors[i].iov_len) {
            break;
        }
    }

    return total;
}

extern "C" off_t lseek(int fd, off_t offset, int whence)
{
    descriptor *entry = file_descriptor(fd);

    if (entry == nullptr) {
        errno = is_console(fd) ? ESPIPE : EBADF;
        return -1;
    }

    off_t target = offset;

    if (whence == SEEK_CUR) {
        target += entry->offset as(off_t);
    } else if (whence == SEEK_END) {
        target += entry->size as(off_t);
    } else if (whence != SEEK_SET) {
        errno = EINVAL;
        return -1;
    }

    if (target < 0 || target as(u64) > entry->size) {
        errno = EINVAL;
        return -1;
    }

    entry->offset = target as(u64);

    return target;
}

extern "C" int close(int fd)
{
    descriptor *entry = file_descriptor(fd);

    if (entry == nullptr) {
        errno = EBADF;
        return -1;
    }

    *entry = {};

    return 0;
}

extern "C" int fstat(int fd, stat *out)
{
    if (out == nullptr) {
        errno = EINVAL;
        return -1;
    }

    if (is_console(fd)) {
        out->mode = S_IFCHR;
        out->size = 0;

        return 0;
    }

    descriptor *entry = file_descriptor(fd);

    if (entry == nullptr) {
        errno = EBADF;
        return -1;
    }

    out->mode = S_IFREG;
    out->size = entry->size as(off_t);

    return 0;
}
