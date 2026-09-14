#ifndef DOOM_OS_PLATFORM_PC_MULTIBOOT2_PLATFORM_HPP_
#define DOOM_OS_PLATFORM_PC_MULTIBOOT2_PLATFORM_HPP_

#include "platform/pc_multiboot2/boot_plan.hpp"
#include "platform/pc_multiboot2/boot_wiring.hpp"
#include "platform/pc_multiboot2/early_console.hpp"

namespace kernel::platform::pc_multiboot2 {

inline constexpr const char *name = "pc_multiboot2";

}  // namespace kernel::platform::pc_multiboot2

#endif  // DOOM_OS_PLATFORM_PC_MULTIBOOT2_PLATFORM_HPP_
