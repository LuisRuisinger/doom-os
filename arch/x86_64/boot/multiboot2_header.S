/* kernel/boot/multiboot2_header.S */

.section .multiboot2, "a"
.align 8

.set MB2_MAGIC, 0xE85250D6 /* Multiboot2 magic number */
.set MB2_ARCH_I386, 0
.set MB2_HEADER_LENGTH, multiboot2_header_end - multiboot2_header_start
.set MB2_CHECKSUM, -(MB2_MAGIC + MB2_ARCH_I386 + MB2_HEADER_LENGTH)

/*
 * Header tag types, and the information tag types we ask to be given.
 *
 * Bit 0 of a tag's flags marks it optional: the bootloader may ignore a tag it cannot
 * honour instead of refusing to boot.
 */
.set MB2_TAG_INFORMATION_REQUEST, 1
.set MB2_TAG_FRAMEBUFFER, 5
.set MB2_TAG_MODULE_ALIGN, 6
.set MB2_TAG_END, 0

.set MB2_TAG_REQUIRED, 0
.set MB2_TAG_OPTIONAL, 1

.set MB2_INFO_MEMORY_MAP, 6

.global multiboot2_header_start
multiboot2_header_start:
    .long MB2_MAGIC
    .long MB2_ARCH_I386
    .long MB2_HEADER_LENGTH
    .long MB2_CHECKSUM

    /*
     * Information request: the memory map, and it is not optional. GRUB supplies one
     * unasked, but nothing in the protocol obliges it to, and the kernel cannot describe
     * physical memory without it - better a bootloader error than a kernel that comes up
     * believing the machine has no RAM.
     */
    .align 8
information_request_tag_start:
    .short MB2_TAG_INFORMATION_REQUEST
    .short MB2_TAG_REQUIRED
    .long information_request_tag_end - information_request_tag_start
    .long MB2_INFO_MEMORY_MAP
information_request_tag_end:

    /*
     * Ask for a linear framebuffer. Without this tag the bootloader leaves the display in
     * text mode and reports that instead, so requesting it is what makes a framebuffer
     * available at all.
     *
     * Zero width, height and depth mean no preference: nothing consumes the framebuffer
     * yet, so there is no mode to prefer. Optional for the same reason - a machine that
     * cannot provide one should still boot, and boot_info already reports its absence
     * through framebuffer.present.
     */
    .align 8
framebuffer_tag_start:
    .short MB2_TAG_FRAMEBUFFER
    .short MB2_TAG_OPTIONAL
    .long framebuffer_tag_end - framebuffer_tag_start
    .long 0 /* width  */
    .long 0 /* height */
    .long 0 /* depth  */
framebuffer_tag_end:

    /*
     * Page-align modules. Their bytes are never copied out of the buffer the bootloader
     * placed them in, so they have to be reserved where they lie; an unaligned module
     * forces every consumer to round outward and reserve a partial page it does not own.
     */
    .align 8
module_align_tag_start:
    .short MB2_TAG_MODULE_ALIGN
    .short MB2_TAG_REQUIRED
    .long module_align_tag_end - module_align_tag_start
module_align_tag_end:

    .align 8
end_tag_start:
    .short MB2_TAG_END
    .short MB2_TAG_REQUIRED
    .long end_tag_end - end_tag_start
end_tag_end:

multiboot2_header_end:
