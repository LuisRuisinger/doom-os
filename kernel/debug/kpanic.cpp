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
    for (;;)
        asm volatile("cli; hlt");
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
    KPRINTLN("  rax: {:#018X}   rbx: {:#018X}   rcx: {:#018X}", r.rax, r.rbx, r.rcx);
    KPRINTLN("  rdx: {:#018X}   rsi: {:#018X}   rdi: {:#018X}", r.rdx, r.rsi, r.rdi);
    KPRINTLN("  rbp: {:#018X}   rsp: {:#018X}   rip: {:#018X}", r.rbp, r.rsp, r.rip);

    KPRINTLN("  r8 : {:#018X}   r9 : {:#018X}   r10: {:#018X}", r.r8, r.r9, r.r10);
    KPRINTLN("  r11: {:#018X}   r12: {:#018X}   r13: {:#018X}", r.r11, r.r12, r.r13);
    KPRINTLN("  r14: {:#018X}   r15: {:#018X}   flg: {:#018X}", r.r14, r.r15, r.rflags);

    KPRINTLN("  cr0: {:#018X}   cr2: {:#018X}   cr3: {:#018X}", r.cr0, r.cr2, r.cr3);
    KPRINTLN("  cr4: {:#018X}   cs : {:#018X}   ss : {:#018X}", r.cr4, r.cs, r.ss);
    KPRINTLN("  ds : {:#018X}   es : {:#018X}   fs : {:#018X}", r.ds, r.es, r.fs);
    KPRINTLN("  gs : {:#018X}", r.gs);
}

[[noreturn]] void kpanic(const char *message, void *frame, const char *file, int line,
                         const char *function) noexcept {
    asm volatile("cli" ::: "memory");

    if (message)
        KPRINTLN("{} {}", detail::PANIC_PREFIX, message);

    KPRINTLN("{} at {}:{} in {}", detail::PANIC_PREFIX, file, line, function);

    if (frame)
        dump_panic_registers(*static_cast<const panic_register_frame *>(frame));

    KPRINTLN("{} halt", detail::PANIC_PREFIX);
    halt();
}

}  // namespace kernel::debug
