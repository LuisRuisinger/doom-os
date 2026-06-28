#ifndef DOOM_OS_KERNEL_CORE_TYPES_HPP_
#define DOOM_OS_KERNEL_CORE_TYPES_HPP_

// =================================================================================================
// Cpp stdlib files
// =================================================================================================

#include <stdarg.h>  // va_list
#include <stddef.h>  // size_t, nullptr_t in C23-ish contexts, offsetof
#include <stdint.h>  // uint8_t, uint16_t, uint32_t, uint64_t

namespace kernel::core {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using usize = size_t;
using isize = intptr_t;

using uptr = uintptr_t;
using iptr = intptr_t;

using paddr_t = u64;
using vaddr_t = u64;

}  // namespace kernel::core

#endif  // DOOM_OS_KERNEL_CORE_TYPES_HPP_