// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/multiboot2/adapter.hpp"
#include "kernel/boot/protocol.hpp"

namespace kernel::boot::active {

bool well_formed()
{
    return multiboot2::adapter::well_formed();
}

kernel::init::init_result parse(info &out)
{
    return multiboot2::adapter::parse(out);
}

}  // namespace kernel::boot::active
