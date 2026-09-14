
#include "kernel/debug/kpanic.hpp"

#include "arch/x86_64/cpu/cpu.hpp"
#include "kernel/core/cast.hpp"
#include "kernel/core/reflect.hpp"
#include "kernel/debug/kprint.hpp"

namespace kernel::debug {

static constexpr kernel::core::usize REGISTERS_PER_LINE = 3;

static void dump_panic_registers(const panic_register_frame &r)
{
    kernel::core::usize column = 0;

    reflect::for_each_field(r, [&](reflect::str_view name, const auto &value) {
        KPRINT("  {:<6} {:#018X}", name, value);

        ++column;
        if (column % REGISTERS_PER_LINE == 0)
            detail::emit_char('\n');
    });

    // The register list does not have to divide evenly; close the line if one is still open.
    if (column % REGISTERS_PER_LINE != 0)
        detail::emit_char('\n');
}

[[noreturn]] void kpanic(const char *message, void *frame, const char *file, int line,
                         const char *function) noexcept
{
    asm volatile("cli" ::: "memory");

    if (message)
        KPRINTLN("{} {}", detail::PANIC_PREFIX, message);

    KPRINTLN("{} at {}:{} in {}", detail::PANIC_PREFIX, file, line, function);
    if (frame)
        dump_panic_registers(*(frame as(const panic_register_frame *)));

    KPRINTLN("{} halt", detail::PANIC_PREFIX);
    arch::x86_64::cpu::halt();
}

}  // namespace kernel::debug
