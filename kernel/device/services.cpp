// =================================================================================================
// Kernel files
// =================================================================================================

#include <uk/services.hpp>

#include "arch/x86_64/serial/io.hpp"
#include "kernel/core/bits.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/debug/kprint.hpp"
#include "kernel/mm/kheap.hpp"
#include "kernel/mm/vmm.hpp"

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

volatile void *map_mmio(paddr_t physical_base, usize size)
{
    return kernel::mm::vmm::map_mmio(physical_base, size).unwrap_or(nullptr);
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
