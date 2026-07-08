#ifndef DOOM_OS_KERNEL_ARCH_X86_64_CORE_PLAN_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_CORE_PLAN_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "exceptions/exceptions.hpp"
#include "kernel/boot/component.hpp"

namespace kernel::arch::x86_64::core_plan {

// =================================================================================================
// Core roots
// =================================================================================================

using core_roots = kernel::boot::type_list<kernel::arch::x86_64::exceptions::core_component>;

}  // namespace kernel::arch::x86_64::core_plan

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_CORE_PLAN_HPP_
