/* config/linker.ld */

ENTRY(_start32)

KERNEL_LMA = 0x00100000;
KERNEL_VMA = 0xffffffff80000000;

/*
 * longmode.S builds the higher-half page tables from its own copy of this address. If the two
 * ever disagree the kernel links at one address and gets mapped at another, which shows up as
 * a triple fault on the jump to the higher half and nothing else. Fail at link time instead.
 */
ASSERT(ASM_KERNEL_VMA == KERNEL_VMA,
       "KERNEL_VMA disagrees between config/linker.ld and kernel/boot/longmode.S")

PF_X = 0x1;
PF_W = 0x2;
PF_R = 0x4;

PHDRS
{
    /*
     * FILEHDR PHDRS makes the first PT_LOAD segment include the ELF header
     * and program headers. This prevents the first loadable section from
     * being pushed too far into the file.
     */
    boot   PT_LOAD FILEHDR PHDRS FLAGS(PF_R | PF_W | PF_X);
    text   PT_LOAD FLAGS(PF_R | PF_X);
    rodata PT_LOAD FLAGS(PF_R);
    data   PT_LOAD FLAGS(PF_R | PF_W);
}

SECTIONS
{
    /*
     * Start after the ELF/program headers inside the first PT_LOAD segment.
     * The Multiboot2 header below must still appear within the first 32 KiB
     * of the final kernel image.
     */
    . = KERNEL_LMA + SIZEOF_HEADERS;

    boot_start = .;

    .boot ALIGN(8) : AT(ADDR(.boot))
    {
        . = ALIGN(8);

        /*
         * GRUB requires the full Multiboot2 header to be 8-byte aligned and
         * fully contained within the first 32 KiB of the image.
         */
        KEEP(*(.multiboot2_header))
        KEEP(*(.multiboot2))
        KEEP(*(.multiboot))

        *(.boot.text .boot.text.*)
        *(.boot.rodata .boot.rodata.*)
        *(.boot.data .boot.data.*)
    } :boot

    .boot.bss ALIGN(4K) (NOLOAD) : AT(ADDR(.boot.bss))
    {
        boot_bss_start = .;
        *(.boot.bss .boot.bss.*)
        boot_bss_end = .;
    } :boot

    . = ALIGN(4K);
    boot_end = .;

    . = KERNEL_VMA + boot_end;

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
