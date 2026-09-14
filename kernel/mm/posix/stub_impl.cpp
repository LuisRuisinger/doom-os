
#include "kernel/core/cast.hpp"
#include "kernel/mm/pmm.hpp"
#include "kernel/mm/vmm.hpp"
#include "posix/abi.hpp"

namespace {

using namespace libos::abi;  // NOLINT(google-build-using-namespace) - this file *is* the ABI

using kernel::core::paddr_t;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

u8 *m_break_base = nullptr;
u64 m_break_used = 0;

constexpr u64 BREAK_SIZE = 2 * 1024 * 1024;
constexpr u64 PAGE_SIZE = 4096;

u64 aligned_up(u64 value, u64 alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

bool ensure_break()
{
    namespace pmm = kernel::mm::pmm;
    namespace vmm = kernel::mm::vmm;

    if (m_break_base != nullptr) {
        return true;
    }

    m_break_base = pmm::alloc(kernel::mm::page_size::SIZE_2M)
                       .map([](paddr_t page) { return vmm::phy_to_vrt(page) as(u8 *); })
                       .unwrap_or(nullptr);

    return m_break_base != nullptr;
}

void zero_bytes(void *buffer, u64 length)
{
    auto *out = buffer as(u8 *);

    for (u64 i = 0; i < length; ++i) {
        out[i] = 0;
    }
}

}  // namespace

extern "C" void *sbrk(ptrdiff_t increment)
{
    if (!ensure_break()) {
        errno = ENOMEM;
        return (-1) as(void *);
    }

    if (increment < 0 || m_break_used + increment as(u64) > BREAK_SIZE) {
        errno = ENOMEM;
        return (-1) as(void *);
    }

    u8                       *previous = m_break_base + m_break_used;
    m_break_used += increment as(u64);

    return previous;
}

extern "C" void *mmap(void *address, size_t length, int protection, int flags, int fd, off_t offset)
{
    constexpr int SUPPORTED_PROTECTION = PROT_READ | PROT_WRITE | PROT_EXEC;
    constexpr int SUPPORTED_FLAGS = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;

    if (address != nullptr || length == 0 || fd != -1 || offset != 0 ||
        (flags & MAP_PRIVATE) == 0 || (flags & MAP_ANONYMOUS) == 0 ||
        (flags & ~SUPPORTED_FLAGS) != 0 || (protection & ~SUPPORTED_PROTECTION) != 0) {
        errno = EINVAL;
        return (-1) as(void *);
    }

    void *current = sbrk(0);

    if (current == (-1) as(void *)) {
        return current;
    }

    const u64 current_address = current as(usize);
    const u64 padding = aligned_up(current_address, PAGE_SIZE) - current_address;

    if (padding != 0 && sbrk(padding as(ptrdiff_t)) == (-1) as(void *)) {
        return (-1) as(void *);
    }

    const u64 allocation = aligned_up(length, PAGE_SIZE);
    void     *memory = sbrk(allocation as(ptrdiff_t));

    if (memory != (-1) as(void *)) {
        zero_bytes(memory, allocation);
    }

    return memory;
}

extern "C" int munmap(void *address, size_t length)
{
    if (address == nullptr || length == 0) {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

extern "C" int mprotect(void *address, size_t length, int protection)
{
    using kernel::mm::has;
    using kernel::mm::page_prot;

    if (address == nullptr || length == 0) {
        errno = EINVAL;
        return -1;
    }

    constexpr int SUPPORTED_PROTECTION = PROT_READ | PROT_WRITE | PROT_EXEC;

    if ((protection & ~SUPPORTED_PROTECTION) != 0) {
        errno = EINVAL;
        return -1;
    }

    const u64 first = address as(u64) & ~(PAGE_SIZE - 1);
    const u64                 last = aligned_up(address as(u64) + length, PAGE_SIZE);

    for (u64 page = first; page < last; page += PAGE_SIZE) {
        const auto resolved = kernel::mm::vmm::vrt_to_phy(page);

        if (resolved.is_err()) {
            errno = ENOMEM;
            return -1;
        }

        const page_prot prot = resolved.unwrap_ref().prot;

        if (protection == PROT_NONE) {
            errno = ENOSYS;
            return -1;
        }

        if ((protection & PROT_WRITE) == 0 && has(prot, page_prot::WRITE)) {
            errno = ENOSYS;
            return -1;
        }

        if ((protection & PROT_EXEC) != 0 && !has(prot, page_prot::EXEC)) {
            errno = ENOSYS;
            return -1;
        }
    }

    return 0;
}

extern "C" void *mremap(void *old_address, size_t old_size, size_t new_size, int flags, ...)
{
    static_cast<void>(old_address);
    static_cast<void>(old_size);
    static_cast<void>(new_size);
    static_cast<void>(flags);

    errno = ENOMEM;
    return (-1) as(void *);
}
