// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/arch/x86_64/serial/io.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/debug/kprint.hpp"

#include <kernel/driver/services.hpp>

namespace kernel::driver::services {

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

void *alloc(usize bytes, usize alignment)
{
    static_cast<void>(bytes);
    static_cast<void>(alignment);

    KPANIC("driver allocation service is not implemented");
}

void free(void *ptr)
{
    if (ptr == nullptr) {
        return;
    }

    KPANIC("driver allocation service is not implemented");
}

volatile void *map_mmio(paddr_t physical_base, usize size)
{
    static_cast<void>(physical_base);
    static_cast<void>(size);

    KPANIC("driver MMIO mapping service is not implemented");
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

dma_buffer dma_alloc(usize bytes, usize alignment)
{
    static_cast<void>(bytes);
    static_cast<void>(alignment);

    KPANIC("driver DMA allocation service is not implemented");
}

void dma_free(dma_buffer buffer)
{
    if (buffer.virtual_address == nullptr && buffer.physical == 0 && buffer.size == 0) {
        return;
    }

    KPANIC("driver DMA allocation service is not implemented");
}

}  // namespace kernel::driver::services
