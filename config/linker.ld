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
        boot_bss_start = .;
        *(.boot.bss .boot.bss.*)
        boot_bss_end = .;
    } :boot

    . = ALIGN(4K);
    boot_end = .;

    . = KERNEL_VMA + (boot_end - KERNEL_LMA);

    kernel_start = .;

    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VMA)
    {
        *(.text .text.*)
        *(.gnu.linkonce.t.*)
    } :kernel

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VMA)
    {
        *(.rodata .rodata.*)
        *(.gnu.linkonce.r.*)
    } :kernel

    .data.rel.ro ALIGN(4K) : AT(ADDR(.data.rel.ro) - KERNEL_VMA)
    {
        *(.data.rel.ro .data.rel.ro.*)
        *(.data.rel.ro.local .data.rel.ro.local.*)
    } :kernel

    .init_array ALIGN(8) : AT(ADDR(.init_array) - KERNEL_VMA)
    {
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    } :kernel

    .fini_array ALIGN(8) : AT(ADDR(.fini_array) - KERNEL_VMA)
    {
        __fini_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array))
        __fini_array_end = .;
    } :kernel

    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VMA)
    {
        *(.data .data.*)
        *(.gnu.linkonce.d.*)
    } :kernel

    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VMA)
    {
        __bss_start = .;

        *(COMMON)
        *(.bss .bss.*)
        *(.gnu.linkonce.b.*)

        __bss_end = .;
    } :kernel

    . = ALIGN(4K);
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