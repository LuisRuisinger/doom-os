// =================================================================================================
// Kernel public files
// =================================================================================================

#include <uk/services.h>
#include <uk/services.hpp>

// =================================================================================================
// C ABI for the driver services
//
// One forwarding body per call, and deliberately nothing else. Every function here is a rename:
// no validation, no translation, no state. A binding that did more than rename would be a second
// implementation to keep in step with the first, and the two would drift the moment either side
// grew a rule the other did not.
//
// bool becomes int because bool's width is a C++ decision this ABI should not export.
// =================================================================================================

namespace {

namespace services = uk::services;

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

}  // extern "C"
