#ifndef DOOM_OS_KERNEL_ARCH_X86_64_TRAP_FRAME_HPP_
#define DOOM_OS_KERNEL_ARCH_X86_64_TRAP_FRAME_HPP_

#include "kernel/core/types.hpp"

namespace kernel::arch::x86_64::trap {

using kernel::core::u64;

struct frame {
    u64 r15;
    u64 r14;
    u64 r13;
    u64 r12;
    u64 r11;
    u64 r10;
    u64 r9;
    u64 r8;
    u64 rbp;
    u64 rdi;
    u64 rsi;
    u64 rdx;
    u64 rcx;
    u64 rbx;
    u64 rax;

    u64 vector;
    u64 error_code;

    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
};

static_assert(sizeof(frame) == 22 * sizeof(u64));

#define DOOM_OS_X86_64_TRAP_FRAME_FIELD(field__, index__) \
    static_assert(__builtin_offsetof(frame, field__) == (index__) * sizeof(u64))

DOOM_OS_X86_64_TRAP_FRAME_FIELD(r15, 0);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(r14, 1);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(r13, 2);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(r12, 3);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(r11, 4);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(r10, 5);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(r9, 6);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(r8, 7);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(rbp, 8);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(rdi, 9);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(rsi, 10);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(rdx, 11);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(rcx, 12);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(rbx, 13);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(rax, 14);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(vector, 15);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(error_code, 16);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(rip, 17);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(cs, 18);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(rflags, 19);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(rsp, 20);
DOOM_OS_X86_64_TRAP_FRAME_FIELD(ss, 21);

#undef DOOM_OS_X86_64_TRAP_FRAME_FIELD

}  // namespace kernel::arch::x86_64::trap

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_TRAP_FRAME_HPP_
