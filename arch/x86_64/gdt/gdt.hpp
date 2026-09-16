#ifndef DOOM_OS_KERNEL_ARCH_X86_64_GDT_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_GDT_HPP_

#include "arch/x86_64/tss/tss.hpp"
#include "kernel/core/array.hpp"
#include "kernel/core/types.hpp"
#include "kernel/init/component.hpp"

namespace kernel::arch::x86_64::gdt {

using kernel::core::u16;
using kernel::core::u64;
using kernel::core::u8;

static constexpr u16 NULL_SELECTOR = 0x00;
static constexpr u16 KERNEL_CODE_SELECTOR = 0x08;
static constexpr u16 KERNEL_DATA_SELECTOR = 0x10;
static constexpr u16 USER_DATA_SELECTOR = 0x18;
static constexpr u16 USER_CODE_SELECTOR = 0x20;
static constexpr u16 TSS_SELECTOR = 0x28;

static constexpr u16 ENTRY_COUNT = 7;

struct [[gnu::packed]] descriptor {
    u16 limit_low;
    u16 base_low;
    u8  base_mid;
    u8  access;
    u8  limit_high_flags;
    u8  base_high;
};

struct [[gnu::packed]] pointer {
    u16 limit;
    u64 base;
};

class table {
    alignas(8) kernel::core::utils::array<descriptor, ENTRY_COUNT> entries_m{};
    pointer ptr_m{};

public:
    void init(const kernel::arch::x86_64::tss::state &task_state_segment);
    void load() const;

    [[nodiscard]] u16 kernel_code_selector() const;
};

void load_task_register(u16 selector);

struct core_component
    : kernel::init::component<core_component, table, kernel::arch::x86_64::tss::core_component> {
    static constexpr auto *name = "GDT";

    template <typename View>
    static kernel::init::init_result init(View view)
    {
        table &self = own(view);

        self.init(dep<kernel::arch::x86_64::tss::core_component>(view));
        self.load();
        load_task_register(TSS_SELECTOR);

        return kernel::core::Ok();
    }
};

}  // namespace kernel::arch::x86_64::gdt

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_GDT_HPP_
