#ifndef DOOM_OS_KERNEL_INCLUDE_KERNEL_DRIVER_TYPES_HPP_
#define DOOM_OS_KERNEL_INCLUDE_KERNEL_DRIVER_TYPES_HPP_

// =================================================================================================
// Cpp stdlib files
// =================================================================================================

#include <stddef.h>
#include <stdint.h>

namespace kernel::driver {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using usize = size_t;
using isize = intptr_t;

using uptr = uintptr_t;
using iptr = intptr_t;

using paddr_t = u64;
using vaddr_t = u64;

}  // namespace kernel::driver

#endif  // DOOM_OS_KERNEL_INCLUDE_KERNEL_DRIVER_TYPES_HPP_
