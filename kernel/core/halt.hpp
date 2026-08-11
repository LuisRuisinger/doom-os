#ifndef DOOM_OS_KERNEL_CORE_HALT_HPP_
#define DOOM_OS_KERNEL_CORE_HALT_HPP_

namespace kernel::core {

// =================================================================================================
// Halt
//
// Stop this core for good: no further instructions, no wakeups. Declared here and implemented
// per architecture, because stopping a CPU is the one thing generic code cannot express.
// =================================================================================================

[[noreturn]] void halt_forever();

}  // namespace kernel::core

#endif  // DOOM_OS_KERNEL_CORE_HALT_HPP_
