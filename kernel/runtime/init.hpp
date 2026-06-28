#ifndef DOOM_OS_KERNEL_RUNTIME_INIT_HPP_
#define DOOM_OS_KERNEL_RUNTIME_INIT_HPP_

namespace kernel::runtime {

// =================================================================================================
// C++ runtime initialization
// =================================================================================================

void call_global_constructors();
void call_global_destructors();

}  // namespace kernel::runtime

#endif  // DOOM_OS_KERNEL_RUNTIME_INIT_HPP_