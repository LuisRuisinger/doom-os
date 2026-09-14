
#include "arch/x86_64/cpu/stacks.hpp"

#include "kernel/core/cast.hpp"

namespace kernel::arch::x86_64::cpu {

static stack describe_stack(u8 *storage, usize storage_size)
{
    const auto bottom = storage as(u64);

    return stack{
        .bottom = bottom,
        .top = bottom + storage_size,
        .size = storage_size,
    };
}

void stack_set::init()
{
    kernel_m = describe_stack(kernel_storage_m, CORE_STACK_SIZE);
    double_fault_m = describe_stack(double_fault_storage_m, CORE_STACK_SIZE);
    nmi_m = describe_stack(nmi_storage_m, CORE_STACK_SIZE);
    machine_check_m = describe_stack(machine_check_storage_m, CORE_STACK_SIZE);
}

}  // namespace kernel::arch::x86_64::cpu
