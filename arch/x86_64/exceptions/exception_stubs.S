/* arch/x86_64/exceptions/exception_stubs.S */

#include "arch/x86_64/trap/entry.inc"

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

X86_64_TRAP_COMMON_ENTRY x86_64_exception_common, x86_64_exception_dispatch

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
