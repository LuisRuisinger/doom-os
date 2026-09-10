#ifndef DOOM_OS_KERNEL_ARCH_X86_64_TRAP_ENTRY_INC_
#define DOOM_OS_KERNEL_ARCH_X86_64_TRAP_ENTRY_INC_

// =================================================================================================
// Common trap entry
//
// Stack on entry to the generated common entry:
//   rsp + 0x00: vector
//   rsp + 0x08: error code
//   rsp + 0x10: rip
//   rsp + 0x18: cs
//   rsp + 0x20: rflags
//   rsp + 0x28: interrupted rsp
//   rsp + 0x30: interrupted ss
//
// The dispatch function receives a pointer to kernel::arch::x86_64::trap::frame in rdi.
// =================================================================================================

.macro X86_64_TRAP_COMMON_ENTRY name__, dispatch__
    .type \name__, @function
\name__:
    cli
    cld

    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp

    /*
     * Keep the C++ call ABI-aligned. IDT entry uses qword pushes, so
     * rsp is expected to be either 16-byte aligned or off by 8 here.
     */
    mov rax, rsp
    and rax, 0x0f
    jz .Lcall_dispatch_\@

    sub rsp, 8
    call \dispatch__
    add rsp, 8
    jmp .Ldispatch_returned_\@

.Lcall_dispatch_\@:
    call \dispatch__

.Ldispatch_returned_\@:
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16
    iretq
.endm

#endif  // DOOM_OS_KERNEL_ARCH_X86_64_TRAP_ENTRY_INC_
