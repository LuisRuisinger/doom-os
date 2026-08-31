// =================================================================================================
// Kernel public files
// =================================================================================================

#include <kernel/driver/services.h>
#include <kernel/driver/services.hpp>

// =================================================================================================
// C ABI for the driver services
//
// One forwarding body per call, and deliberately nothing else. Every function here is a rename:
// no validation, no translation, no state. A binding that did more than rename would be a second
// implementation to keep in step with the first, and the two would drift the moment either side
// grew a rule the other did not.
//
// bool becomes int because bool's width is a C++ decision this ABI should not export. dma_buffer
// crosses as its own C struct rather than the C++ type, with a layout assertion below, because a
// caller in another language reads the C header and must be able to trust it without reading the
// C++ one.
// =================================================================================================

namespace {

namespace services = kernel::driver::services;

static_assert(sizeof(doom_os_dma_buffer) == sizeof(services::dma_buffer),
              "the C and C++ dma_buffer have drifted in size");
static_assert(alignof(doom_os_dma_buffer) == alignof(services::dma_buffer),
              "the C and C++ dma_buffer have drifted in alignment");
static_assert(offsetof(doom_os_dma_buffer, physical) == offsetof(services::dma_buffer, physical),
              "the C and C++ dma_buffer have drifted at physical");
static_assert(offsetof(doom_os_dma_buffer, virtual_address) ==
                  offsetof(services::dma_buffer, virtual_address),
              "the C and C++ dma_buffer have drifted at virtual_address");
static_assert(offsetof(doom_os_dma_buffer, size) == offsetof(services::dma_buffer, size),
              "the C and C++ dma_buffer have drifted at size");

}  // namespace

extern "C" {

void doom_os_driver_log(const char *message)
{
    services::log(message);
}

void doom_os_driver_io_write8(uint16_t port, uint8_t value)
{
    services::io_write8(port, value);
}

uint8_t doom_os_driver_io_read8(uint16_t port)
{
    return services::io_read8(port);
}

void *doom_os_driver_alloc(size_t bytes, size_t alignment)
{
    return services::alloc(bytes, alignment);
}

void doom_os_driver_free(void *ptr)
{
    services::free(ptr);
}

volatile void *doom_os_driver_map_mmio(uint64_t physical_base, size_t size)
{
    return services::map_mmio(physical_base, size);
}

int doom_os_driver_register_irq(uint32_t vector, doom_os_irq_handler handler, void *context)
{
    return services::register_irq(vector, handler, context) ? 1 : 0;
}

void doom_os_driver_unregister_irq(uint32_t vector, doom_os_irq_handler handler, void *context)
{
    services::unregister_irq(vector, handler, context);
}

doom_os_dma_buffer doom_os_driver_dma_alloc(size_t bytes, size_t alignment)
{
    const services::dma_buffer buffer = services::dma_alloc(bytes, alignment);

    return doom_os_dma_buffer{buffer.physical, buffer.virtual_address, buffer.size};
}

void doom_os_driver_dma_free(doom_os_dma_buffer buffer)
{
    services::dma_free(
        services::dma_buffer{buffer.physical, buffer.virtual_address, buffer.size});
}

}  // extern "C"
