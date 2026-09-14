// =================================================================================================
// POSIX ABI files
// =================================================================================================

#include "posix/abi.hpp"

// =================================================================================================
// Kernel files
// =================================================================================================

#include "arch/x86_64/mmu/direct_map.hpp"
#include "arch/x86_64/mmu/mmu.hpp"
#include "kernel/mm/pmm.hpp"

namespace {

using namespace libos::abi;  // NOLINT(google-build-using-namespace) - this file *is* the ABI

using kernel::core::paddr_t;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

// =================================================================================================
// Program break
//
// One 2 MiB page, taken on first use and never grown. sbrk's contract is a single contiguous
// region that only moves forward, which is exactly what a large page already is - and a heap that
// cannot grow is honest here, because nothing below this file can relocate one that has been
// handed out.
// =================================================================================================

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
    namespace mmu = kernel::arch::x86_64::mmu;

    if (m_break_base != nullptr) {
        return true;
    }

    m_break_base =
        pmm::alloc_page(kernel::mm::page_size::SIZE_2M)
            .map([](paddr_t page) { return static_cast<u8 *>(mmu::phy_to_vrt(page)); })
            .unwrap_or(static_cast<u8 *>(nullptr));

    return m_break_base != nullptr;
}

void zero_bytes(void *buffer, u64 length)
{
    auto *out = static_cast<u8 *>(buffer);

    for (u64 i = 0; i < length; ++i) {
        out[i] = 0;
    }
}

}  // namespace

// =================================================================================================
// Memory
// =================================================================================================

extern "C" void *sbrk(ptrdiff_t increment)
{
    if (!ensure_break()) {
        errno = ENOMEM;
        return reinterpret_cast<void *>(-1);
    }

    if (increment < 0 || m_break_used + static_cast<u64>(increment) > BREAK_SIZE) {
        errno = ENOMEM;
        return reinterpret_cast<void *>(-1);
    }

    u8 *previous = m_break_base + m_break_used;
    m_break_used += static_cast<u64>(increment);

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
        return reinterpret_cast<void *>(-1);
    }

    void *current = sbrk(0);

    if (current == reinterpret_cast<void *>(-1)) {
        return current;
    }

    const u64 current_address = reinterpret_cast<usize>(current);
    const u64 padding = aligned_up(current_address, PAGE_SIZE) - current_address;

    if (padding != 0 && sbrk(static_cast<ptrdiff_t>(padding)) == reinterpret_cast<void *>(-1)) {
        return reinterpret_cast<void *>(-1);
    }

    const u64 allocation = aligned_up(length, PAGE_SIZE);
    void     *memory = sbrk(static_cast<ptrdiff_t>(allocation));

    if (memory != reinterpret_cast<void *>(-1)) {
        zero_bytes(memory, allocation);
    }

    return memory;
}

// Succeeds and reclaims nothing, which is a leak rather than a wrong answer: the caller asked to
// stop using the range and it may, it simply will not get the memory back. sbrk is the only source
// below this and its break moves one way, so reclaiming needs a virtual address allocator to hand
// ranges out and take them back - the same one map_mmio is waiting on. Reporting failure instead
// would be worse, because a caller that cannot unmap usually cannot proceed either.
extern "C" int munmap(void *address, size_t length)
{
    if (address == nullptr || length == 0) {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

// =================================================================================================
// mprotect
//
// Answered, not applied. There is one address space, and the memory this file hands out lives in
// the direct map - so taking write access away here would take it away from the kernel too, on
// bytes the kernel reaches by formula and has no reason to expect to lose. The page walker can
// split a 2 MiB leaf to reach 4 KiB granularity, so the obstacle is the sharing rather than the
// granularity.
//
// What is left is still worth doing, because it is true: read the mapping back and report whether
// it already grants what was asked for. A caller wanting no more than it has gets a success that
// means something, and one wanting more is told no instead of being told yes and finding out at
// the fault. Nothing is relaxed either - the direct map is writable and NX before this call and
// after it, so a request to drop write or to add execute is refused rather than ignored.
// =================================================================================================

extern "C" int mprotect(void *address, size_t length, int protection)
{
    namespace mmu = kernel::arch::x86_64::mmu;

    if (address == nullptr || length == 0) {
        errno = EINVAL;
        return -1;
    }

    constexpr int SUPPORTED_PROTECTION = PROT_READ | PROT_WRITE | PROT_EXEC;

    if ((protection & ~SUPPORTED_PROTECTION) != 0) {
        errno = EINVAL;
        return -1;
    }

    const u64 first = reinterpret_cast<u64>(address) & ~(PAGE_SIZE - 1);
    const u64 last = aligned_up(reinterpret_cast<u64>(address) + length, PAGE_SIZE);

    for (u64 page = first; page < last; page += PAGE_SIZE) {
        const mmu::mapping resolved = mmu::vrt_to_phy(page);

        if (!resolved.present) {
            errno = ENOMEM;
            return -1;
        }

        const u64 flags = resolved.flags.word(0);

        // PROT_NONE asks for access to be taken away, which is the one thing this cannot do at
        // all: the page stays readable and writable whatever is returned here.
        if (protection == PROT_NONE) {
            errno = ENOSYS;
            return -1;
        }

        if ((protection & PROT_WRITE) == 0 && (flags & mmu::PAGE_FLAG_WRITABLE) != 0) {
            errno = ENOSYS;
            return -1;
        }

        if ((protection & PROT_EXEC) != 0 && (flags & mmu::PAGE_FLAG_NO_EXECUTE) != 0) {
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
    return reinterpret_cast<void *>(-1);
}
