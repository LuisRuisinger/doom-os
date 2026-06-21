/* kernel/boot/multiboot2_header.S */

.section .multiboot2, "a"
.align 8

.set MB2_MAGIC, 0xE85250D6 /* Multiboot2 magic number */
.set MB2_ARCH_I386, 0
.set MB2_HEADER_LENGTH, multiboot2_header_end - multiboot2_header_start
.set MB2_CHECKSUM, -(MB2_MAGIC + MB2_ARCH_I386 + MB2_HEADER_LENGTH)

.global multiboot2_header_start
multiboot2_header_start:
    .long MB2_MAGIC
    .long MB2_ARCH_I386
    .long MB2_HEADER_LENGTH
    .long MB2_CHECKSUM

    /* end tag */
    .short 0
    .short 0
    .long 8

multiboot2_header_end: