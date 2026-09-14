// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/boot_info.hpp"

#include "platform/pc_multiboot2/boot_wiring.hpp"

namespace kernel::boot::boot_info {

namespace {

kernel::boot::handoff m_handoff{};
kernel::boot::info    m_info{};

}  // namespace

// =================================================================================================
// Handoff
// =================================================================================================

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

// =================================================================================================
// Boot description
// =================================================================================================

const kernel::boot::info &current()
{
    return m_info;
}

bool available()
{
    return m_info.valid;
}

// =================================================================================================
// Component
//
// Parses straight into the stored description: an info is around ten kilobytes, which is too
// much to stage on the kernel stack.
// =================================================================================================

kernel::init::init_result component::parse()
{
    m_info.reset();

    auto outcome = kernel::boot::active_boot_protocol::parse(m_handoff, m_info);

    if (outcome.is_err()) {
        m_info.reset();
        return outcome;
    }

    // Checked once here rather than at each push, so the adapters stay a straight
    // translation. A machine with more regions than the tables hold is a machine we cannot
    // describe, and a partial description is indistinguishable from a complete one once
    // valid is set.
    if (m_info.truncated()) {
        m_info.reset();
        return kernel::core::Err(kernel::init::init_error::CAPACITY_EXCEEDED);
    }

    m_info.valid = true;
    return kernel::core::Ok();
}

}  // namespace kernel::boot::boot_info
