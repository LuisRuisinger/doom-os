#ifndef DOOM_OS_KERNEL_CORE_UTILS_CONTAINER_HPP_
#define DOOM_OS_KERNEL_CORE_UTILS_CONTAINER_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"

namespace kernel::core::utils {

enum class container_error : u8 {
    CAPACITY_EXCEEDED,
    EMPTY,
};

}  // namespace kernel::core::utils

#endif  // DOOM_OS_KERNEL_CORE_UTILS_CONTAINER_HPP_
