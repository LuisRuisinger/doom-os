/* kernel/boot/boot32.S */

.intel_syntax noprefix

// =================================================================================================
// 32-bit bootstrap stack
// =================================================================================================

.section .boot.bss

.align 16
stack_bottom:
    .skip 16384

.align 16
stack_top:

// =================================================================================================
// 32-bit entry point
// =================================================================================================

.section .boot.text
.code32

.global _start32
.type _start32, @function

.extern enter_long_mode

_start32:
    cli

    mov esp, offset stack_top

    /*
     * GRUB / Multiboot2 provides:
     *   eax = Multiboot2 magic
     *   ebx = Multiboot2 info pointer
     *
     * enter_long_mode consumes eax/ebx directly.
     */
    call enter_long_mode

.hang:
    hlt
    jmp .hang