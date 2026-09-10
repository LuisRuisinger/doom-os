// =================================================================================================
// Kernel files
// =================================================================================================

#include "arch/x86_64/mmu/mmu.hpp"
#include "arch/x86_64/serial/io.hpp"
#include "kernel/core/bits.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/debug/kprint.hpp"
#include "kernel/mm/kheap.hpp"

#include <uk/services.hpp>

namespace uk::services {

void log(const char *message)
{
    KPRINTLN("[driver] {}", message);
}

void io_write8(u16 port, u8 value)
{
    kernel::arch::x86_64::outb(port, value);
}

u8 io_read8(u16 port)
{
    return kernel::arch::x86_64::inb(port);
}

// The kernel heap, not a pool of its own. Drivers are trusted and share this address space, so a
// second space would buy accounting rather than isolation - and isolation only with a footprint
// limit on top of it. See kernel/mm/kheap.hpp.
void *alloc(usize bytes, usize alignment)
{
    return kernel::mm::kheap::alloc(bytes, alignment);
}

void free(void *ptr)
{
    kernel::mm::kheap::free(ptr);
}

// =================================================================================================
// MMIO
//
// A formula, not an allocation. The aperture at physical P is placed at MMIO_MAP_BASE + P, which
// is injective - two devices cannot be handed the same address, and a device mapped twice gets the
// address it got the first time, so nothing has to remember what was mapped. It is the same trick
// the direct map plays for RAM, with the cache disabled instead of enabled, on the range the
// direct map deliberately leaves out (arch/x86_64/mmu/mmu.cpp).
//
// Uniformly 4 KiB. Device apertures are not 2 MiB aligned in general, and a uniform leaf size is
// also what makes an overlapping second call harmless: map_range treats a repeat with the same
// frame and flags as a no-op, which stops holding the moment two callers pick different sizes.
//
// The returned pointer is volatile for the compiler and the mapping is uncached for the CPU. Both
// are needed: one stops loads being folded away, the other stops them being answered from cache.
// =================================================================================================

volatile void *map_mmio(paddr_t physical_base, usize size)
{
    namespace mmu = kernel::arch::x86_64::mmu;

    using kernel::core::utils::align_down;
    using kernel::core::utils::align_up;

    if (size == 0 || physical_base >= mmu::MMIO_MAP_SIZE ||
        size > mmu::MMIO_MAP_SIZE - physical_base) {
        return nullptr;
    }

    const paddr_t first = align_down<paddr_t>(physical_base, mmu::PAGE_SIZE_4K);
    const paddr_t last = align_up<paddr_t>(physical_base + size, mmu::PAGE_SIZE_4K);

    mmu::page_flags flags{};
    flags.word(0) = mmu::PAGE_FLAG_WRITABLE | mmu::PAGE_FLAG_CACHE_DISABLE |
                    mmu::PAGE_FLAG_NO_EXECUTE | mmu::PAGE_FLAG_GLOBAL;

    if (!mmu::map_range(mmu::MMIO_MAP_BASE + first, first, last - first, mmu::page_size::SIZE_4K,
                        flags)) {
        return nullptr;
    }

    return reinterpret_cast<volatile void *>(mmu::MMIO_MAP_BASE + physical_base);
}

bool register_irq(u32 vector, irq_handler handler, void *context)
{
    static_cast<void>(vector);
    static_cast<void>(handler);
    static_cast<void>(context);

    KPANIC("driver IRQ registration service is not implemented");
}

void unregister_irq(u32 vector, irq_handler handler, void *context)
{
    static_cast<void>(vector);
    static_cast<void>(context);

    if (handler == nullptr) {
        return;
    }

    KPANIC("driver IRQ registration service is not implemented");
}

}  // namespace uk::services
