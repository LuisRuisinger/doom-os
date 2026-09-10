#ifndef DOOM_OS_INCLUDE_UK_SERVICES_HPP_
#define DOOM_OS_INCLUDE_UK_SERVICES_HPP_

// =================================================================================================
// Kernel public files
// =================================================================================================

#include <uk/types.hpp>

namespace uk::services {

// =================================================================================================
// Driver-facing kernel services
//
// Drivers are trusted in the first unikernel design, but this header is the API surface they
// should target. The implementation can later move behind stricter memory or capability policy
// without requiring drivers to include internal kernel subsystems directly.
// =================================================================================================

using irq_handler = void (*)(void *context);

void log(const char *message);

void io_write8(u16 port, u8 value);
[[nodiscard]] u8 io_read8(u16 port);

[[nodiscard]] void *alloc(usize bytes, usize alignment);
void free(void *ptr);

[[nodiscard]] volatile void *map_mmio(paddr_t physical_base, usize size);

[[nodiscard]] bool register_irq(u32 vector, irq_handler handler, void *context);
void unregister_irq(u32 vector, irq_handler handler, void *context);

}  // namespace uk::services

#endif  // DOOM_OS_INCLUDE_UK_SERVICES_HPP_
