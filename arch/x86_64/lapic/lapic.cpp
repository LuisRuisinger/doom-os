// =================================================================================================
// Kernel files
// =================================================================================================

#include "arch/x86_64/lapic/lapic.hpp"

#include "arch/x86_64/core_init.hpp"
#include "kernel/acpi/madt.hpp"
#include "kernel/mm/page.hpp"
#include "kernel/mm/vmm.hpp"

namespace kernel::arch::x86_64::lapic {

namespace {

vaddr_t m_lapic_base_vaddr{};
paddr_t m_lapic_base_paddr{};

inline constexpr u8 SPURIOUS_INTERRUPT_VECTOR = 0xFF;

inline void barrier()
{
    asm volatile("" ::: "memory");
}

}  // namespace

// =================================================================================================
// Component Lifecycle
// =================================================================================================

kernel::init::init_result component::init_lapic()
{
    const auto &madt = kernel::acpi::get_madt_info();

    kernel::init::init_result lapic_init = lapic::init(madt.lapic_address);
    if (lapic_init.is_err())
        return lapic_init;

    if (!kernel::arch::x86_64::core_init::init_late_bsp())
        return kernel::core::Err(kernel::init::init_error::UNSPECIFIED);

    return kernel::core::Ok();
}

bool is_ready()
{
    return m_lapic_base_vaddr != 0;
}

kernel::init::init_result init(paddr_t lapic_base_address)
{
    if (lapic_base_address == 0)
        return kernel::core::Err(kernel::init::init_error::INVALID_BOOT_DATA);

    if (m_lapic_base_vaddr != 0) {
        if (m_lapic_base_paddr == lapic_base_address)
            return kernel::core::Ok();

        return kernel::core::Err(kernel::init::init_error::INVALID_BOOT_DATA);
    }

    auto mapping = kernel::mm::vmm::map_mmio(lapic_base_address, kernel::mm::PAGE_SIZE_4K);
    if (mapping.is_err())
        return kernel::core::Err(kernel::init::init_error::NO_USABLE_MEMORY);

    m_lapic_base_paddr = lapic_base_address;
    m_lapic_base_vaddr = mapping.unwrap_ref();

    return kernel::core::Ok();
}

kernel::init::init_result init_core()
{
    if (m_lapic_base_vaddr == 0)
        return kernel::core::Err(kernel::init::init_error::DEPENDENCY_UNAVAILABLE);

    // Clear Task Priority Register: accept all interrupt classes
    write(reg::tpr, 0);

    // Flat model for destination format
    write(reg::dfr, 0xFFFFFFFF);
    write(reg::ldr, (read(reg::ldr) & 0x00FFFFFF) | 1);

    // Mask legacy LINT0 and LINT1 lines
    write(reg::lvt_lint0, LVT_MASKED);
    write(reg::lvt_lint1, LVT_MASKED);

    // Mask performance counter and thermal monitoring registers
    write(reg::lvt_perf, LVT_MASKED);
    write(reg::lvt_thermal, LVT_MASKED);

    // Configure error interrupt vector and reset ESR
    write(reg::lvt_error, 0xFE);
    write(reg::esr, 0);
    write(reg::esr, 0);

    // Enable the APIC hardware and assign the spurious interrupt vector
    write(reg::svr, SVR_APIC_ENABLE | SPURIOUS_INTERRUPT_VECTOR);

    // Clear any dangling in-service interrupts
    eoi();

    return kernel::core::Ok();
}

// =================================================================================================
// Hardware I/O Primitives
// =================================================================================================

u32 read(reg r)
{
    barrier();
    const auto *address = reinterpret_cast<const volatile u32 *>(
        m_lapic_base_vaddr + static_cast<u32>(r)
    );
    const u32 value = *address;
    barrier();
    return value;
}

void write(reg r, u32 value)
{
    barrier();
    auto *address = reinterpret_cast<volatile u32 *>(
        m_lapic_base_vaddr + static_cast<u32>(r)
    );
    *address = value;
    barrier();
}

void eoi()
{
    write(reg::eoi, 0);
}

u32 id()
{
    return read(reg::id) >> 24;
}

u32 version()
{
    return read(reg::version) & 0xFF;
}

// =================================================================================================
// Inter-Processor Interrupts (IPI) & SMP Bootstrap
// =================================================================================================

void send_ipi(u32 dest_apic_id, u32 vector, u32 flags)
{
    write(reg::icr_high, dest_apic_id << 24);
    write(reg::icr_low, (vector & 0xFF) | flags);

    while ((read(reg::icr_low) & ICR_STATUS_PENDING) != 0) {
        asm volatile("pause" ::: "memory");
    }
}

void send_init_ipi(u32 dest_apic_id)
{
    send_ipi(dest_apic_id, 0, ICR_DELIVERY_INIT | ICR_LEVEL_ASSERT);
}

void send_startup_ipi(u32 dest_apic_id, u8 vector)
{
    send_ipi(dest_apic_id, vector, ICR_DELIVERY_STARTUP | ICR_LEVEL_ASSERT);
}

// =================================================================================================
// APIC Timer
// =================================================================================================

void configure_timer(u8 vector, u32 initial_count, bool periodic, u8 divide_value)
{
    write(reg::timer_div, divide_value & 0x0F);

    u32 lvt = vector;
    if (periodic)
        lvt |= LVT_TIMER_PERIODIC;

    write(reg::lvt_timer, lvt);
    write(reg::timer_init, initial_count);
}

void stop_timer()
{
    write(reg::lvt_timer, LVT_MASKED);
    write(reg::timer_init, 0);
}

}  // namespace kernel::arch::x86_64::lapic
