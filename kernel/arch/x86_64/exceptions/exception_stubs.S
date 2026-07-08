/* kernel/arch/x86_64/exceptions/exception_stubs.S */

.intel_syntax noprefix

.section .text, "ax"
.code64

.extern x86_64_exception_dispatch

// =================================================================================================
// Stub macros
// =================================================================================================

.macro EXCEPTION_STUB_NO_ERROR vector
    .global x86_64_exception_stub_\vector
    .type x86_64_exception_stub_\vector, @function
x86_64_exception_stub_\vector:
    push 0
    push \vector
    jmp x86_64_exception_common
.endm

.macro EXCEPTION_STUB_ERROR vector
    .global x86_64_exception_stub_\vector
    .type x86_64_exception_stub_\vector, @function
x86_64_exception_stub_\vector:
    push \vector
    jmp x86_64_exception_common
.endm

// =================================================================================================
// Exception stubs
// =================================================================================================

EXCEPTION_STUB_NO_ERROR 0
EXCEPTION_STUB_NO_ERROR 1
EXCEPTION_STUB_NO_ERROR 2
EXCEPTION_STUB_NO_ERROR 3
EXCEPTION_STUB_NO_ERROR 4
EXCEPTION_STUB_NO_ERROR 5
EXCEPTION_STUB_NO_ERROR 6
EXCEPTION_STUB_NO_ERROR 7
EXCEPTION_STUB_ERROR 8
EXCEPTION_STUB_NO_ERROR 9
EXCEPTION_STUB_ERROR 10
EXCEPTION_STUB_ERROR 11
EXCEPTION_STUB_ERROR 12
EXCEPTION_STUB_ERROR 13
EXCEPTION_STUB_ERROR 14
EXCEPTION_STUB_NO_ERROR 15
EXCEPTION_STUB_NO_ERROR 16
EXCEPTION_STUB_ERROR 17
EXCEPTION_STUB_NO_ERROR 18
EXCEPTION_STUB_NO_ERROR 19
EXCEPTION_STUB_NO_ERROR 20
EXCEPTION_STUB_ERROR 21
EXCEPTION_STUB_NO_ERROR 22
EXCEPTION_STUB_NO_ERROR 23
EXCEPTION_STUB_NO_ERROR 24
EXCEPTION_STUB_NO_ERROR 25
EXCEPTION_STUB_NO_ERROR 26
EXCEPTION_STUB_NO_ERROR 27
EXCEPTION_STUB_NO_ERROR 28
EXCEPTION_STUB_ERROR 29
EXCEPTION_STUB_ERROR 30
EXCEPTION_STUB_NO_ERROR 31

// =================================================================================================
// Common entry
// =================================================================================================
//
// Stack on entry to x86_64_exception_common:
//   rsp + 0x00: vector
//   rsp + 0x08: error code
//   rsp + 0x10: rip
//   rsp + 0x18: cs
//   rsp + 0x20: rflags
//   rsp + 0x28: interrupted rsp
//   rsp + 0x30: interrupted ss
//
// =================================================================================================

.type x86_64_exception_common, @function
x86_64_exception_common:
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
     * Keep the C++ call ABI-aligned. Exception entry uses qword pushes, so
     * rsp is expected to be either 16-byte aligned or off by 8 here.
     */
    mov rax, rsp
    and rax, 0x0f
    jz .Lcall_dispatch

    sub rsp, 8
    call x86_64_exception_dispatch
    add rsp, 8
    jmp .Ldispatch_returned

.Lcall_dispatch:
    call x86_64_exception_dispatch

.Ldispatch_returned:
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

// =================================================================================================
// Stub table
// =================================================================================================

.section .rodata, "a"
.align 8

.global x86_64_exception_stub_table
.type x86_64_exception_stub_table, @object

x86_64_exception_stub_table:
    .irp vector, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, \
                  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
        .quad x86_64_exception_stub_\vector
    .endr

.size x86_64_exception_stub_table, . - x86_64_exception_stub_table

.section .note.GNU-stack, "", @progbits
