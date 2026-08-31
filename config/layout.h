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

/*
 * Permanent physical direct map. Physical address P is reachable at
 * DOOM_OS_DIRECT_MAP_BASE + P once the VMM switches away from the early boot tables.
 *
 * The base is 512 GiB aligned so a single PML4 slot owns the whole region.
 */
#define DOOM_OS_DIRECT_MAP_BASE 0xffff800000000000

/*
 * How much the early page tables cover, from DOOM_OS_KERNEL_VMA and from 0. One page directory
 * of 2 MiB entries, so this caps at 1 GiB. The linker script asserts the kernel image fits.
 */
#define DOOM_OS_EARLY_MAP_SIZE 0x40000000

/*
 * The most physical memory the frame allocator will manage. Its bitmaps are statically sized
 * from this and live in .bss, which is why it is a ceiling rather than a discovered value:
 * raising it costs roughly 1 bit per 4 KiB frame, so 16 GiB costs about 520 KiB of kernel
 * image. Memory beyond it is reported and ignored, never handed out.
 *
 * The linker script asserts the kernel image fits DOOM_OS_EARLY_MAP_SIZE, so the two are
 * related: a ceiling large enough to push .bss past 1 GiB fails the build rather than
 * silently producing a kernel the early page tables cannot map.
 */
#define DOOM_OS_MAX_PHYSICAL_MEMORY 0x400000000

#endif /* DOOM_OS_CONFIG_LAYOUT_H_ */
