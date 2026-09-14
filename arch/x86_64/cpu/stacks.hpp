#ifndef DOOM_OS_KERNEL_ARCH_X86_64_CPU_STACKS_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_CPU_STACKS_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"
#include "kernel/init/component.hpp"

namespace kernel::arch::x86_64::cpu {

using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

// =================================================================================================
// Constants
// =================================================================================================

static constexpr usize CORE_STACK_SIZE = 16 * 1024;
static constexpr usize STACK_BYTE_ALIGNMENT = 16;

// =================================================================================================
// CPU-local stack descriptor
// =================================================================================================

struct stack {
    u64   bottom;
    u64   top;
    usize size;
};

// =================================================================================================
// CPU-local stacks
//
// The storage the TSS points RSP0 and its IST entries at. Owned by stacks_component; every
// other component sees it const, which is what makes handing the stack tops to the TSS safe.
// =================================================================================================

class stack_set {
    stack kernel_m{};
    stack double_fault_m{};
    stack nmi_m{};
    stack machine_check_m{};

    alignas(STACK_BYTE_ALIGNMENT) u8 kernel_storage_m[CORE_STACK_SIZE]{};
    alignas(STACK_BYTE_ALIGNMENT) u8 double_fault_storage_m[CORE_STACK_SIZE]{};
    alignas(STACK_BYTE_ALIGNMENT) u8 nmi_storage_m[CORE_STACK_SIZE]{};
    alignas(STACK_BYTE_ALIGNMENT) u8 machine_check_storage_m[CORE_STACK_SIZE]{};

public:
    void init();

    [[nodiscard]] const stack &kernel() const
    {
        return kernel_m;
    }
    [[nodiscard]] const stack &double_fault() const
    {
        return double_fault_m;
    }
    [[nodiscard]] const stack &nmi() const
    {
        return nmi_m;
    }
    [[nodiscard]] const stack &machine_check() const
    {
        return machine_check_m;
    }
};

// =================================================================================================
// Core component
// =================================================================================================

struct stacks_component : kernel::init::component<stacks_component, stack_set> {
    static constexpr auto *name = "STACKS";

    template <typename View>
    static kernel::init::init_result init(View view)
    {
        own(view).init();
        return kernel::core::Ok();
    }
};

}  // namespace kernel::arch::x86_64::cpu

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_CPU_STACKS_HPP_
