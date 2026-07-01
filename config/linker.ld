/* kernel/arch/x86_64/linker.ld */

ENTRY(_start32)

KERNEL_LMA = 0x00100000;
KERNEL_VMA = 0xffffffff80000000;

PF_X = 0x1;
PF_W = 0x2;
PF_R = 0x4;

PHDRS
{
    boot   PT_LOAD FLAGS(PF_R | PF_W | PF_X);
    text   PT_LOAD FLAGS(PF_R | PF_X);
    rodata PT_LOAD FLAGS(PF_R);
    data   PT_LOAD FLAGS(PF_R | PF_W);
}

SECTIONS
{
    . = KERNEL_LMA;

    boot_start = .;

    .boot ALIGN(4K) : AT(ADDR(.boot))
    {
        . = ALIGN(8);

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
    } :text

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VMA)
    {
        *(.rodata .rodata.*)
        *(.gnu.linkonce.r.*)
    } :rodata

    /*
     * Keep these writable for now.
     * They are often logically read-only after relocation/init,
     * but for a freestanding kernel bring-up it is safer not to
     * make them R-only yet.
     */
    .data.rel.ro ALIGN(4K) : AT(ADDR(.data.rel.ro) - KERNEL_VMA)
    {
        *(.data.rel.ro .data.rel.ro.*)
        *(.data.rel.ro.local .data.rel.ro.local.*)
    } :data

    .init_array ALIGN(8) : AT(ADDR(.init_array) - KERNEL_VMA)
    {
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    } :data

    .fini_array ALIGN(8) : AT(ADDR(.fini_array) - KERNEL_VMA)
    {
        __fini_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array))
        __fini_array_end = .;
    } :data

    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VMA)
    {
        *(.data .data.*)
        *(.gnu.linkonce.d.*)

        *(.got .got.*)
        *(.got.plt .got.plt.*)
    } :data

    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VMA)
    {
        __bss_start = .;

        *(COMMON)
        *(.bss .bss.*)
        *(.gnu.linkonce.b.*)

        __bss_end = .;
    } :data

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
        *(.note.GNU-stack)
    }
}