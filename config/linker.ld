/* kernel/arch/x86_64/linker.ld */

ENTRY(_start32)

KERNEL_LMA = 0x00100000;
KERNEL_VMA = 0xffffffff80000000;

PHDRS
{
    boot   PT_LOAD FLAGS(0x7);
    kernel PT_LOAD FLAGS(0x7);
}

SECTIONS
{
    . = KERNEL_LMA;

    boot_start = .;

    .boot ALIGN(4K) : AT(ADDR(.boot))
    {
        KEEP(*(.multiboot2))
        *(.boot.text .boot.text.*)
        *(.boot.rodata .boot.rodata.*)
        *(.boot.data .boot.data.*)
    } :boot

    .boot.bss ALIGN(4K) : AT(ADDR(.boot.bss))
    {
        *(.boot.bss .boot.bss.*)
    } :boot

    boot_end = .;

    . = KERNEL_VMA + (boot_end - KERNEL_LMA);

    kernel_start = .;

    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VMA)
    {
        *(.text .text.*)
    } :kernel

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VMA)
    {
        *(.rodata .rodata.*)
    } :kernel

    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VMA)
    {
        *(.data .data.*)
    } :kernel

    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VMA)
    {
        *(COMMON)
        *(.bss .bss.*)
    } :kernel

    kernel_end = .;

    kernel_physical_start = kernel_start - KERNEL_VMA;
    kernel_physical_end = kernel_end - KERNEL_VMA;

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.comment)
        *(.note .note.*)
    }
}