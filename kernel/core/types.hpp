#ifndef DOOM_OS_KERNEL_CORE_TYPES_HPP_
#define DOOM_OS_KERNEL_CORE_TYPES_HPP_

#include <stdarg.h>  // va_list

#include <uk/types.hpp>

namespace kernel::core {

using uk::u16;
using uk::u32;
using uk::u64;
using uk::u8;

using uk::i16;
using uk::i32;
using uk::i64;
using uk::i8;

using uk::isize;
using uk::usize;

using uk::iptr;
using uk::uptr;

using uk::paddr_t;
using uk::vaddr_t;

}  // namespace kernel::core

#endif  // DOOM_OS_KERNEL_CORE_TYPES_HPP_
