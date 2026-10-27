
#include "kernel/core/bits.hpp"
#include "kernel/core/cast.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/mm/vmm.hpp"
#include "kernel/runtime/memory.hpp"
#include "posix/abi.hpp"

namespace {

using namespace libos::abi;  // NOLINT(google-build-using-namespace) - this file *is* the ABI

using kernel::core::u64;
using kernel::core::utils::align_down;
using kernel::core::utils::align_up;
using kernel::mm::FRAME_SIZE;
using kernel::mm::page_prot;

namespace vmm = kernel::mm::vmm;

}  // namespace

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

    const auto allocated = vmm::alloc(length, page_prot::KERNEL_DATA);

    if (allocated.is_err()) {
        errno = ENOMEM;
        return (-1) as(void *);
    }

    void *memory = allocated.unwrap_ref();

    memset(memory, 0, align_up<u64>(length, FRAME_SIZE));

    if ((protection & PROT_WRITE) != 0)
        return memory;

    page_prot prot = page_prot::GLOBAL;

    if ((protection & PROT_EXEC) != 0)
        prot |= page_prot::EXEC;

    if (vmm::protect(memory, length, prot).is_err())
        KPANIC("mmap: cannot drop write access on a fresh allocation at {}", memory);

    return memory;
}

extern "C" int munmap(void *address, size_t length)
{
    if (vmm::free(address, length).is_ok())
        return 0;

    errno = EINVAL;
    return -1;
}

extern "C" int mprotect(void *address, size_t length, int protection)
{
    constexpr int SUPPORTED_PROTECTION = PROT_READ | PROT_WRITE | PROT_EXEC;

    if ((protection & ~SUPPORTED_PROTECTION) != 0) {
        errno = EINVAL;
        return -1;
    }

    page_prot prot = page_prot::GLOBAL;

    if ((protection & PROT_WRITE) != 0)
        prot |= page_prot::WRITE;

    if ((protection & PROT_EXEC) != 0)
        prot |= page_prot::EXEC;

    const u64 first = align_down<u64>(address as(u64), FRAME_SIZE);
    const u64 last = align_up<u64>(address as(u64) + length, FRAME_SIZE);

    if (vmm::protect(first as(void *), last - first, prot).is_err()) {
        errno = ENOMEM;
        return -1;
    }

    return 0;
}
