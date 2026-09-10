#ifndef DOOM_OS_INCLUDE_UK_TYPES_HPP_
#define DOOM_OS_INCLUDE_UK_TYPES_HPP_

// =================================================================================================
// Cpp stdlib files
// =================================================================================================

#include <stddef.h>
#include <stdint.h>

namespace uk {

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

static_assert(sizeof(u8) == 1);
static_assert(sizeof(u16) == 2);
static_assert(sizeof(u32) == 4);
static_assert(sizeof(u64) == 8);
static_assert(sizeof(paddr_t) == 8);
static_assert(sizeof(vaddr_t) == 8);

}  // namespace uk

#endif  // DOOM_OS_INCLUDE_UK_TYPES_HPP_
