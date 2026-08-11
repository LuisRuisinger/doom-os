#ifndef DOOM_OS_CONFIG_LAYOUT_H_
#define DOOM_OS_CONFIG_LAYOUT_H_

/*
 * Where the kernel lives.
 *
 * Included by both the linker script and the early boot assembly, so moving the kernel means
 * changing one number here. Preprocessor defines only - this file is consumed by cpp on
 * behalf of ld, by the assembler, and potentially by C++, so it must not contain anything
 * that is syntax in only one of them.
 *
 * DOOM_OS_KERNEL_VMA must stay 1 GiB aligned: the early page tables map the higher half with
 * a single page directory filled from its first entry.
 */

#define DOOM_OS_KERNEL_LMA 0x00100000
#define DOOM_OS_KERNEL_VMA 0xffffffff80000000

#endif /* DOOM_OS_CONFIG_LAYOUT_H_ */
