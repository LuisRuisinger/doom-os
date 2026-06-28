// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/debug/kpanic.hpp"

#include "../arch/x86_64/cpu/cpu.hpp"
#include "kernel/debug/kprint.hpp"

namespace kernel::debug {

// =================================================================================================
// Halt
// =================================================================================================

[[noreturn]] static void halt() {
    for (;;) asm volatile("cli; hlt");
}

// =================================================================================================
// Kernel panic
// =================================================================================================

using kernel::arch::x86_64::cpu::cpu_register;
using kernel::arch::x86_64::cpu::CPU_REGISTER_COUNT;
using kernel::arch::x86_64::cpu::cpu_register_value;
using kernel::arch::x86_64::cpu::read_current_registers;

using kernel::core::usize;

static void dump_panic_registers(const panic_register_frame &r) {
    KPRINTLN("  rax: {:x}    rbx: {:x}    rcx: {:x}", r.rax, r.rbx, r.rcx);
    KPRINTLN("  rdx: {:x}    rsi: {:x}    rdi: {:x}", r.rdx, r.rsi, r.rdi);
    KPRINTLN("  rbp: {:x}    rsp: {:x}    rip: {:x}", r.rbp, r.rsp, r.rip);

    KPRINTLN("  r8 : {:x}    r9 : {:x}    r10: {:x}", r.r8, r.r9, r.r10);
    KPRINTLN("  r11: {:x}    r12: {:x}    r13: {:x}", r.r11, r.r12, r.r13);
    KPRINTLN("  r14: {:x}    r15: {:x}    flg: {:x}", r.r14, r.r15, r.rflags);

    KPRINTLN("  cr0: {:x}    cr2: {:x}    cr3: {:x}", r.cr0, r.cr2, r.cr3);
    KPRINTLN("  cr4: {:x}    cs : {:x}    ss : {:x}", r.cr4, r.cs, r.ss);
    KPRINTLN("  ds : {:x}    es : {:x}    fs : {:x}", r.ds, r.es, r.fs);
    KPRINTLN("  gs : {:x}", r.gs);
}

[[noreturn]] void kpanic(const char *message, void *frame, const char *file, int line,
                         const char *function) noexcept {
    asm volatile("cli" ::: "memory");

    static constexpr auto *panic_prefix = "\x1b[1;31m[panic]\x1b[0m";
    if (message)
        KPRINTLN("{} {}", panic_prefix, message);

    KPRINTLN("{} at {}:{} in {}", panic_prefix, file, line, function);

    if (frame)
        dump_panic_registers(*static_cast<const panic_register_frame *>(frame));

    KPRINTLN("{} halt", panic_prefix);
    halt();
}

}  // namespace kernel::debug