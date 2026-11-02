#include "kernel/boot/boot_info.hpp"

#include "platform/pc_multiboot2/boot_wiring.hpp"

namespace kernel::boot::boot_info {

namespace {

kernel::boot::info m_info{};

}  // namespace

const kernel::boot::info &current()
{
    return m_info;
}

kernel::init::init_result component::parse()
{
    m_info.reset();

    auto outcome = kernel::boot::active_boot_protocol::parse(m_info);

    if (outcome.is_err())
        m_info.reset();

    return outcome;
}

}  // namespace kernel::boot::boot_info
