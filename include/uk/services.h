#ifndef DOOM_OS_INCLUDE_UK_SERVICES_H_
#define DOOM_OS_INCLUDE_UK_SERVICES_H_

/* =================================================================================================
 * Driver-facing kernel services, C ABI
 *
 * The same surface as services.hpp, spelled so that anything with a C FFI can reach it. That is
 * the whole point of it existing separately: C++ mangles, and a mangled name is a contract only a
 * C++ compiler of the right version can honour. These names are flat, so Rust, Zig, C and Ada bind
 * to them by writing them down.
 *
 * The kernel implements this once, in kernel/device/services_abi.cpp, forwarding to the C++
 * definitions. It is the C++ header that is the wrapper here, not this one: this is the ABI, and
 * services.hpp is the ergonomic spelling of it for callers that happen to be C++.
 *
 * Nothing here allocates a callback thunk or takes ownership of a pointer. An IRQ handler is a
 * plain function pointer plus an opaque context word, which is the one shape every FFI agrees on.
 * ============================================================================================== */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*doom_os_irq_handler)(void *context);

void doom_os_driver_log(const char *message);

void    doom_os_driver_io_write8(uint16_t port, uint8_t value);
uint8_t doom_os_driver_io_read8(uint16_t port);

void *doom_os_driver_alloc(size_t bytes, size_t alignment);
void  doom_os_driver_free(void *ptr);

volatile void *doom_os_driver_map_mmio(uint64_t physical_base, size_t size);

int  doom_os_driver_register_irq(uint32_t vector, doom_os_irq_handler handler, void *context);
void doom_os_driver_unregister_irq(uint32_t vector, doom_os_irq_handler handler, void *context);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* DOOM_OS_INCLUDE_UK_SERVICES_H_ */
