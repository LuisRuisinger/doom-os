/* kernel/arch/x86_64/idt_load.S */

.intel_syntax noprefix

.section .text, "ax"
.code64

.global idt_load
.type idt_load, @function

// =================================================================================================
// Load IDT
// =================================================================================================
//
// SysV x86_64 calling convention:
//   rdi = pointer to IDT pointer
//
// =================================================================================================

idt_load:
    lidt [rdi]
    ret