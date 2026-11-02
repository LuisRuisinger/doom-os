#include "kernel/boot/boot_info.hpp"

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

    if (!kernel::boot::active::well_formed())
        return kernel::core::Err(kernel::init::init_error::INVALID_BOOT_DATA);

    auto outcome = kernel::boot::active::parse(m_info);

    if (outcome.is_err())
        m_info.reset();

    return outcome;
}

}  // namespace kernel::boot::boot_info
