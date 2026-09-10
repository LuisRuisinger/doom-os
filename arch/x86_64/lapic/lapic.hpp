#ifndef DOOM_OS_ARCH_X86_64_LAPIC_LAPIC_HPP_
#define DOOM_OS_ARCH_X86_64_LAPIC_LAPIC_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "arch/x86_64/idt/idt.hpp"
#include "kernel/acpi/acpi.hpp"
#include "kernel/core/types.hpp"
#include "kernel/init/component.hpp"

namespace kernel::arch::x86_64::lapic {

using kernel::core::paddr_t;
using kernel::core::u16;
using kernel::core::u32;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;
using kernel::core::vaddr_t;

// =================================================================================================
// Strongly-Typed Register Offsets
// =================================================================================================

enum class reg : u32 {
    id            = 0x0020,
    version       = 0x0030,
    tpr           = 0x0080,
    apr           = 0x0090,
    ppr           = 0x00A0,
    eoi           = 0x00B0,
    rrd           = 0x00C0,
    ldr           = 0x00D0,
    dfr           = 0x00E0,
    svr           = 0x00F0,
    isr_base      = 0x0100, // 8 registers: 0x100 - 0x170 (stride 0x10)
    tmr_base      = 0x0180, // 8 registers: 0x180 - 0x1F0
    irr_base      = 0x0200, // 8 registers: 0x200 - 0x270
    esr           = 0x0280,
    icr_low       = 0x0300,
    icr_high      = 0x0310,
    lvt_timer     = 0x0320,
    lvt_thermal   = 0x0330,
    lvt_perf      = 0x0340,
    lvt_lint0     = 0x0350,
    lvt_lint1     = 0x0360,
    lvt_error     = 0x0370,
    timer_init    = 0x0380,
    timer_current = 0x0390,
    timer_div     = 0x03E0,
};

[[nodiscard]] inline constexpr reg isr_register(usize index)
{
    return static_cast<reg>(static_cast<u32>(reg::isr_base) + (index * 0x10));
}

[[nodiscard]] inline constexpr reg tmr_register(usize index)
{
    return static_cast<reg>(static_cast<u32>(reg::tmr_base) + (index * 0x10));
}

[[nodiscard]] inline constexpr reg irr_register(usize index)
{
    return static_cast<reg>(static_cast<u32>(reg::irr_base) + (index * 0x10));
}

// =================================================================================================
// Architectural Bit Flags
// =================================================================================================

inline constexpr u32 SVR_APIC_ENABLE      = 1 << 8;
inline constexpr u32 LVT_MASKED           = 1 << 16;
inline constexpr u32 LVT_TIMER_PERIODIC   = 1 << 17;
inline constexpr u32 ICR_DELIVERY_INIT    = 0x5 << 8;
inline constexpr u32 ICR_DELIVERY_STARTUP = 0x6 << 8;
inline constexpr u32 ICR_LEVEL_ASSERT     = 1 << 14;
inline constexpr u32 ICR_STATUS_PENDING   = 1 << 12;

// =================================================================================================
// Component & Initialization Interface
// =================================================================================================

[[nodiscard]] bool is_ready();
kernel::init::init_result init(paddr_t lapic_base_address);
kernel::init::init_result init_core();

struct component : kernel::init::component<component, kernel::init::no_resource,
                                           kernel::acpi::component> {
    static constexpr const char *name = "LAPIC";

    static kernel::init::init_result init_lapic();

    template <typename View>
    static kernel::init::init_result init(View)
    {
        return init_lapic();
    }
};

struct core_component
    : kernel::init::component<core_component, kernel::init::no_resource,
                              kernel::arch::x86_64::idt::core_component> {
    static constexpr const char *name = "LAPIC_CORE";

    template <typename View>
    static kernel::init::init_result init(View view)
    {
        (void)dep<kernel::arch::x86_64::idt::core_component>(view);
        return init_core();
    }
};

void eoi();

// =================================================================================================
// Register Access & Commands
// =================================================================================================

[[nodiscard]] u32 read(reg r);
void write(reg r, u32 value);

[[nodiscard]] u32 id();
[[nodiscard]] u32 version();

void send_ipi(u32 dest_apic_id, u32 vector, u32 flags = 0);
void send_init_ipi(u32 dest_apic_id);
void send_startup_ipi(u32 dest_apic_id, u8 vector);

void configure_timer(u8 vector, u32 initial_count, bool periodic, u8 divide_value = 0x3);
void stop_timer();

}  // namespace kernel::arch::x86_64::lapic

#endif  // DOOM_OS_ARCH_X86_64_LAPIC_LAPIC_HPP_
