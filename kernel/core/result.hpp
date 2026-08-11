#ifndef DOOM_OS_KERNEL_CORE_RESULT_HPP_
#define DOOM_OS_KERNEL_CORE_RESULT_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/debug/kpanic.hpp"

// =================================================================================================
// Result<T, E>
//
// Single point where the third-party Result is configured and pulled in, so the failure hooks
// are wired the same way everywhere. Without this the library traps with __builtin_trap() in
// freestanding builds, which costs the file, line and register dump that kpanic already gives.
//
// Include this header rather than <result/result.hpp> directly.
// =================================================================================================

#ifndef RESULT_ERROR
#    define RESULT_ERROR(message__) KPANIC(message__)
#endif

// Niche packing looks for a value a one-byte enum cannot hold, and by default probes all 256
// exhaustively - each probe a __PRETTY_FUNCTION__ parse. Measured on init_error, that single
// instantiation cost ~130 ms, which every translation unit naming a component paid.
//
// Kernel error enums are small, so a sentinel turns up in the first handful of candidates.
// Probing fewer keeps the packing and drops the cost. If an enum ever does use up this many
// values the library falls back to a flag byte, which costs a byte and nothing else.
#ifndef RESULT_SMALL_ENUM_SENTINEL_PROBE_SEQUENCE
#    define RESULT_SMALL_ENUM_SENTINEL_PROBE_SEQUENCE 32
#endif

#include <result/result.hpp>

namespace kernel::core {

using lsr::Err;
using lsr::Ok;
using lsr::Result;

}  // namespace kernel::core

#endif  // DOOM_OS_KERNEL_CORE_RESULT_HPP_
