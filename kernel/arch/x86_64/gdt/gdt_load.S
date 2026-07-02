/* kernel/arch/x86_64/gdt_load.S */

.intel_syntax noprefix

.section .text, "ax"
.code64

.global gdt_load
.type gdt_load, @function

// =================================================================================================
// Load GDT
// =================================================================================================
//
// SysV x86_64 calling convention:
//   rdi = pointer to GDT pointer
//   rsi = kernel code selector
//   rdx = kernel data selector
//
// =================================================================================================

gdt_load:
    lgdt [rdi]

    mov ax, dx
    mov ds, ax
    mov es, ax
    mov ss, ax

    xor ax, ax
    mov fs, ax
    mov gs, ax

    /*
     * Reload CS.
     *
     * CS cannot be written with mov. We use a far return:
     *   push new_cs
     *   push target_rip
     *   lretq
     */
    lea rax, [rip + .reload_cs]
    push rsi
    push rax
    lretq

.reload_cs:
    ret
