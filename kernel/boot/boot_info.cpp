
#include "kernel/boot/boot_info.hpp"

#include "platform/pc_multiboot2/boot_wiring.hpp"

namespace kernel::boot::boot_info {

namespace {

kernel::boot::handoff m_handoff{};
kernel::boot::info    m_info{};

}  // namespace

void set_handoff(kernel::boot::handoff source)
{
    m_handoff = source;
    m_info.reset();
}

void set_handoff(u64 magic, paddr_t address)
{
    set_handoff(kernel::boot::handoff{
        .magic = magic,
        .address = address,
    });
}

const kernel::boot::handoff &current_handoff()
{
    return m_handoff;
}

const kernel::boot::info &current()
{
    return m_info;
}

bool available()
{
    return m_info.valid;
}

kernel::init::init_result component::parse()
{
    m_info.reset();

    auto outcome = kernel::boot::active_boot_protocol::parse(m_handoff, m_info);

    if (outcome.is_err()) {
        m_info.reset();
        return outcome;
    }

    if (m_info.truncated()) {
        m_info.reset();
        return kernel::core::Err(kernel::init::init_error::CAPACITY_EXCEEDED);
    }

    m_info.valid = true;
    return kernel::core::Ok();
}

}  // namespace kernel::boot::boot_info
