// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/boot/boot_plan.hpp"
#include "kernel/boot/init.hpp"
#include "kernel/core/types.hpp"
#include "kernel/debug/kpanic.hpp"
#include "kernel/debug/kprint.hpp"
#include "kernel/runtime/init.hpp"

namespace kernel::core {

struct base {
    virtual void f() = 0;
    virtual ~base();
};

struct derived : base {
    void f() override {}
    ~derived() override = default;
};

[[gnu::noinline]] static void call_virtual(base *object) { object->f(); }

base::~base() { call_virtual(this); }

// =================================================================================================
// Kernel longmode entry point
// =================================================================================================

void kernel_main64(u64 mb2_magic [[maybe_unused]], u64 mb2_info [[maybe_unused]]) {
    kernel::boot::run_init_graph_silent<kernel::boot::early_boot_roots>();
    KPRINTLN("KERNEL BOOT");

    kernel::runtime::call_global_constructors();
    kernel::boot::run_init_graph_or_halt<kernel::boot::boot_roots>();

    KPANIC();
}

}  // namespace kernel::core

// =================================================================================================
// C ABI wrapper
// =================================================================================================

extern "C" void kernel_main64(kernel::core::u64 mb2_magic, kernel::core::u64 mb2_info) {
    kernel::core::kernel_main64(mb2_magic, mb2_info);
}