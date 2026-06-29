// https://www.rpi4os.com/part1-bootstrapping/

// ***************************************
// SCTLR_EL1, System Control Register (EL1)
// Architecture Reference Manual Section D17.2.118
// ***************************************
#define SCTLR_RESERVED (3 << 28) | (3 << 22) | (1 << 20) | (1 << 11)
#define USER_MASK_ACCESS (1 << 9)
#define SCTLR_WFE_WFI_ENABLED (1 << 18) | (1 << 16)
#define SCTLR_VALUE_MMU_DISABLED (SCTLR_RESERVED | USER_MASK_ACCESS | SCTLR_WFE_WFI_ENABLED)

// ***************************************
// HCR_EL2, Hypervisor Configuration Register (EL2)
// Architecture Reference Manual Section D17.2.48
// ***************************************
#define HCR_RW (1 << 31)

// ***************************************
// SPSR_EL2, Saved Program Status Register (EL2)
// Architecture Reference Manual Section C5.2.19
// ***************************************
#define SPSR_MASK_ALL (7 << 6)
#define SPSR_EL1      (5 << 0)
#define SPSR_VALUE (SPSR_MASK_ALL | SPSR_EL1)


// ensure the linker puts this at the start of the kernel image
.section ".text.boot"
.global _start
_start:
    // CPU 0 continues; CPUs 1–3 are released later via spin table (see smp.c).
    mrs  x0, mpidr_el1
    and  x0, x0, #3
    cbnz x0, exit

    // are we already in EL1?
    mrs  x1, CurrentEL
    and  x1, x1, #8
    cbz  x1, el1_entry

    // otherwise, switch to EL1 by fake exception to return from
    ldr x2, =HCR_RW
    msr hcr_el2, x2

    ldr x3, =SPSR_VALUE
    msr spsr_el2, x3

    adr x4, el1_entry
    msr elr_el2, x4

    eret // -> el1_entry

el1_entry:
    // Cortex-A72: enable cache coherency with the other cores in the cluster
    // (CPUECTLR_EL1.SMPEN, bit 6). Must be set before caches/MMU come on, or
    // shared data + the LDXR/STXR spinlock are not coherent across CPUs.
    mrs x0, S3_1_c15_c2_1
    orr x0, x0, #(1 << 6)
    msr S3_1_c15_c2_1, x0
    isb

    // Configure processor (MMU off under QEMU -kernel).
    ldr x2, =(SCTLR_VALUE_MMU_DISABLED)
    mrs x3, sctlr_el1
    orr x2, x2, x3
    msr sctlr_el1, x2

    // mask-out exceptions at EL1
    msr DAIFSet, #0b1111
    // initialize SP
    msr SPSel, #1
    ldr     x0, =stackend
    mov     sp, x0   // x0 should still be the stackend address
    
    // initialize Exception Vector
    ldr x1, = vector_table_el1
    msr VBAR_EL1, x1

    // Jump to our main() routine in C/C++
    bl      kmain // x0 should still be the stackend address
	// I need to modify something here
.global EXIT
EXIT:
    msr daifset, #0xf   /* disable all interrupts so we don't fault in WFI */

exit:
    wfi
    b    exit

.section ".bss"

.balign 16
stack:
    .rept 0x010000
    .byte 0
    .endr
.global stackend
stackend:
    .rept 0x140000 // 0x010000 mult 20 for a max of 20 user programs
    .byte 0
    .endr // This will be the stacks for our userprograms
e0stackend:

