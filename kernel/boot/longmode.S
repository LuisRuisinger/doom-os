.section .boot.text
.code32

.global enter_long_mode
.type enter_long_mode, @function

// =================================================================================================
// Constants
// =================================================================================================

.set KERNEL_VMA, 0xffffffff80000000

/*
 * Paging indices derived from KERNEL_VMA rather than written out, so moving the kernel means
 * changing one number. The linker script has to agree on that number; it exports its own and
 * asserts against ASM_KERNEL_VMA below, which turns a mismatch into a link error instead of a
 * triple fault on the jump to the higher half.
 */
.set PML4_INDEX, (KERNEL_VMA >> 39) & 0x1FF
.set PDPT_INDEX, (KERNEL_VMA >> 30) & 0x1FF
.set PD_INDEX, (KERNEL_VMA >> 21) & 0x1FF

.global ASM_KERNEL_VMA
.set ASM_KERNEL_VMA, KERNEL_VMA

/* fill_1gib_page_directory maps the whole directory from its first entry. */
.if PD_INDEX != 0
.error "KERNEL_VMA must be 1 GiB aligned"
.endif

.set GDT64_KERNEL_CODE, 0x08
.set GDT64_KERNEL_DATA, 0x10

.set CR0_PG, 0x80000000
.set CR4_PAE, 0x00000020

.set MSR_EFER, 0xC0000080
.set EFER_LME, 0x00000100

.set PAGE_PRESENT, 0x001
.set PAGE_WRITE,   0x002
.set PAGE_SIZE,    0x080

// =================================================================================================
// Enter long mode
// =================================================================================================

enter_long_mode:
    cli

    /*
     * Multiboot2:
     *   eax = magic
     *   ebx = info pointer
     */
    movl %eax, boot_mb2_magic
    movl %ebx, boot_mb2_info

    call setup_boot_page_tables

    lgdt gdt64_pointer32

    movl $pml4_table, %eax
    movl %eax, %cr3

    movl %cr4, %eax
    orl $CR4_PAE, %eax
    movl %eax, %cr4

    movl $MSR_EFER, %ecx
    rdmsr
    orl $EFER_LME, %eax
    wrmsr

    movl %cr0, %eax
    orl $CR0_PG, %eax
    movl %eax, %cr0

    ljmp $GDT64_KERNEL_CODE, $long_mode_entry

// =================================================================================================
// Temporary page table setup
// =================================================================================================

setup_boot_page_tables:
    cld

    /*
     * Clear all boot paging structures:
     *   pml4_table
     *   pdpt_low
     *   pd_low
     *   pdpt_high
     *   pd_high
     *
     * 5 pages * 4096 bytes = 20480 bytes = 5120 dwords
     */
    movl $pml4_table, %edi
    xorl %eax, %eax
    movl $5120, %ecx
    rep stosl

    /*
     * Low identity map:
     *   pml4[0] -> pdpt_low
     *   pdpt_low[0] -> pd_low
     */
    movl $pdpt_low, %eax
    orl $(PAGE_PRESENT | PAGE_WRITE), %eax
    movl %eax, pml4_table
    movl $0, pml4_table + 4

    movl $pd_low, %eax
    orl $(PAGE_PRESENT | PAGE_WRITE), %eax
    movl %eax, pdpt_low
    movl $0, pdpt_low + 4

    /*
     * Higher-half map:
     *
     *   pml4[PML4_INDEX] -> pdpt_high
     *   pdpt_high[PDPT_INDEX] -> pd_high
     *
     * For the default KERNEL_VMA of 0xffffffff80000000 those work out to 511 and 510.
     */
    movl $pdpt_high, %eax
    orl $(PAGE_PRESENT | PAGE_WRITE), %eax
    movl %eax, pml4_table + PML4_INDEX * 8
    movl $0, pml4_table + PML4_INDEX * 8 + 4

    movl $pd_high, %eax
    orl $(PAGE_PRESENT | PAGE_WRITE), %eax
    movl %eax, pdpt_high + PDPT_INDEX * 8
    movl $0, pdpt_high + PDPT_INDEX * 8 + 4

    /*
     * Fill both page directories with 2 MiB mappings.
     *
     * Low:
     *   0x00000000 -> 0x00000000
     *
     * High:
     *   0xffffffff80000000 -> 0x00000000
     */
    movl $pd_low, %edi
    call fill_1gib_page_directory

    movl $pd_high, %edi
    call fill_1gib_page_directory

    ret

fill_1gib_page_directory:
    movl $(PAGE_PRESENT | PAGE_WRITE | PAGE_SIZE), %eax
    xorl %edx, %edx
    movl $512, %ecx

.map_pd_entry:
    movl %eax, (%edi)
    movl %edx, 4(%edi)

    addl $0x200000, %eax
    adcl $0, %edx

    addl $8, %edi
    loop .map_pd_entry

    ret

// =================================================================================================
// 64-bit entry
// =================================================================================================

.code64

.type long_mode_entry, @function

long_mode_entry:
    movw $GDT64_KERNEL_DATA, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    movw $0, %ax
    movw %ax, %fs
    movw %ax, %gs

    /*
     * Temporary low identity-mapped stack.
     *
     * Later your kernel can switch to a proper higher-half kernel stack.
     */
    leaq boot_stack_top(%rip), %rsp

    /*
     * SysV x86_64 calling convention:
     *   rdi = mb2_magic
     *   rsi = mb2_info
     */
    movl boot_mb2_magic(%rip), %edi
    movl boot_mb2_info(%rip), %esi

    /*
     * kernel_main64 is linked in the higher half.
     *
     * Use movabs because the target is a high canonical address.
     */
    movabsq $kernel_main64, %rax
    call *%rax

.hang:
    hlt
    jmp .hang

// =================================================================================================
// Temporary boot data
// =================================================================================================

.section .boot.data

.align 8
boot_mb2_magic:
    .long 0

boot_mb2_info:
    .long 0

// =================================================================================================
// Temporary GDT
// =================================================================================================

.align 8
gdt64:
    .quad 0x0000000000000000
    .quad 0x00AF9A000000FFFF
    .quad 0x00CF92000000FFFF

gdt64_end:

gdt64_pointer32:
    .word gdt64_end - gdt64 - 1
    .long gdt64

// =================================================================================================
// Temporary page tables and stack
// =================================================================================================

.section .boot.bss, "aw", @nobits

.align 4096
pml4_table:
    .skip 4096

.align 4096
pdpt_low:
    .skip 4096

.align 4096
pd_low:
    .skip 4096

.align 4096
pdpt_high:
    .skip 4096

.align 4096
pd_high:
    .skip 4096

.align 16
boot_stack_bottom:
    .skip 16384

.align 16
boot_stack_top:
