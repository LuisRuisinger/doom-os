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
 * Permanent MMIO window. A device aperture at physical address P is reachable at
 * DOOM_OS_MMIO_MAP_BASE + P, uncached, once a driver has asked for it.
 *
 * A second window rather than a wider direct map, because the two want opposite flags. The direct
 * map deliberately covers described memory only, so that an aperture never gains a writeback
 * cached alias; this is the alias those apertures do get, with the cache disabled.
 *
 * It needs no allocator for the same reason the direct map does not: base + P is injective, so two
 * devices cannot collide and mapping one twice yields the same address. Unlike the direct map it
 * carries no static tables - nothing is mapped until asked for, and the levels are allocated then.
 *
 * The base is 512 GiB aligned so a single PML4 slot owns the window, and it is the slot after the
 * direct map's.
 */
#define DOOM_OS_MMIO_MAP_BASE 0xffff808000000000

/*
 * The highest physical address map_mmio will accept an aperture at. A bound on requests rather
 * than a reservation: the window costs nothing until a page in it is mapped.
 */
#define DOOM_OS_MMIO_MAP_SIZE 0x8000000000

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
