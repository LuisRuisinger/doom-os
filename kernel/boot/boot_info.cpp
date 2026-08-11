// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/boot_info.hpp"

#include "kernel/boot/boot_wiring.hpp"

namespace kernel::boot::boot_info {

namespace {

kernel::boot::handoff g_handoff{};
kernel::boot::info    g_info{};

}  // namespace

// =================================================================================================
// Handoff
// =================================================================================================

void set_handoff(kernel::boot::handoff source)
{
    g_handoff = source;
    g_info.reset();
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
    return g_handoff;
}

// =================================================================================================
// Boot description
// =================================================================================================

const kernel::boot::info &current()
{
    return g_info;
}

bool available()
{
    return g_info.valid;
}

// =================================================================================================
// Component
//
// Parses straight into the stored description: an info is around ten kilobytes, which is too
// much to stage on the kernel stack.
// =================================================================================================

init_result component::parse()
{
    g_info.reset();

    auto outcome = kernel::boot::active_boot_protocol::parse(g_handoff, g_info);

    if (outcome.is_err()) {
        g_info.reset();
        return outcome;
    }

    g_info.valid = true;
    return kernel::boot::Ok();
}

}  // namespace kernel::boot::boot_info
