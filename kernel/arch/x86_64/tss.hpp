#ifndef DOOM_OS_KERNEL_ARCH_X86_64_TSS_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_TSS_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/component.hpp"
#include "kernel/core/types.hpp"

namespace kernel::arch::x86_64::cpu {

struct local_state;

}  // namespace kernel::arch::x86_64::cpu

namespace kernel::arch::x86_64::tss {

using kernel::core::u16;
using kernel::core::u32;
using kernel::core::u64;
using kernel::core::u8;

// =================================================================================================
// Constants
// =================================================================================================

static constexpr u8 PRIVILEGE_STACK_COUNT = 3;
static constexpr u8 INTERRUPT_STACK_COUNT = 7;
static constexpr u8 NO_INTERRUPT_STACK = 0;

enum class interrupt_stack : u8 {
    NONE = 0,
    DOUBLE_FAULT = 1,
    NMI = 2,
    MACHINE_CHECK = 3,
    IST4 = 4,
    IST5 = 5,
    IST6 = 6,
    IST7 = 7,
};

// =================================================================================================
// 64-bit task state segment layout
// =================================================================================================

struct [[gnu::packed]] segment {
    u32 reserved0;
    u64 rsp[PRIVILEGE_STACK_COUNT];
    u64 reserved1;
    u64 ist[INTERRUPT_STACK_COUNT];
    u64 reserved2;
    u16 reserved3;
    u16 io_map_base;
};

static_assert(sizeof(segment) == 104);
static_assert(__builtin_offsetof(segment, rsp) == 4);
static_assert(__builtin_offsetof(segment, ist) == 36);
static_assert(__builtin_offsetof(segment, io_map_base) == 102);

static constexpr u16 IO_MAP_DISABLED_BASE = sizeof(segment);

// =================================================================================================
// Initialization
// =================================================================================================

struct stack_config {
    u64 rsp0{};
    u64 rsp1{};
    u64 rsp2{};

    // interrupt stack table
    u64 ist1{};
    u64 ist2{};
    u64 ist3{};
    u64 ist4{};
    u64 ist5{};
    u64 ist6{};
    u64 ist7{};
};

class state {
   public:
    void clear();

    bool set_privilege_stack(u8 privilege_level, u64 stack_top);

    bool set_interrupt_stack(interrupt_stack stack, u64 stack_top);

    void init(const stack_config &config);

    const segment &layout() const;

    u64 base() const;

    static constexpr u16 limit() { return sizeof(segment) - 1; }

   private:
    segment layout_m{};
};

// =================================================================================================
// Core component
// =================================================================================================

struct core_component
    : kernel::boot::context_component_base<core_component, kernel::arch::x86_64::cpu::local_state> {
    static constexpr const char *name = "TSS";

    static bool init_component(kernel::arch::x86_64::cpu::local_state &cpu);
};

}  // namespace kernel::arch::x86_64::tss

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_TSS_HPP_
