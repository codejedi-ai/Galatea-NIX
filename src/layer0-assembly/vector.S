

#include "config.h"

.text
.balign 0x800
.global vector_table_el1
vector_table_el1: // This is our EL1 exception vector

/* Save callee-saved + argument GPRs onto the current EL1 stack. */
.macro SAVE_GPRS
	stp x29,  x30,  [sp, #-16]!
	stp x27,  x28,  [sp, #-16]!
	stp x25,  x26,  [sp, #-16]!
	stp x23,  x24,  [sp, #-16]!
	stp x21,  x22,  [sp, #-16]!
	stp x19, x20, [sp, #-16]!
	stp x17, x18, [sp, #-16]!
	stp x15, x16, [sp, #-16]!
	stp x13, x14, [sp, #-16]!
	stp x11, x12, [sp, #-16]!
	stp x9, x10, [sp, #-16]!
	stp x7, x8, [sp, #-16]!
	stp x5, x6, [sp, #-16]!
	stp x3, x4, [sp, #-16]!
	stp x1, x2, [sp, #-16]!
	str x0, [sp, #-8]!
.endm

.macro KERNEL_DISPATCH handler
	SAVE_GPRS
	mov x0, sp
	bl \handler
	/* NOT REACHED */
	b .
.endm

	el1_sp0_s:
	// msr SPSel, #0 // We use the Kernel Stack Pointer
	stp x29,  x30,  [sp, #-16]!
	stp x27,  x28,  [sp, #-16]!
	stp x25,  x26,  [sp, #-16]!
	stp x23,  x24,  [sp, #-16]!
	stp x21,  x22,  [sp, #-16]!
	stp x19, x20, [sp, #-16]!
	stp x17, x18, [sp, #-16]!
	stp x15, x16, [sp, #-16]!
	stp x13, x14, [sp, #-16]!
	stp x11, x12, [sp, #-16]!
	stp x9, x10, [sp, #-16]!
	stp x7, x8, [sp, #-16]!
	stp x5, x6, [sp, #-16]!
	stp x3, x4, [sp, #-16]!
	stp x1, x2, [sp, #-16]!
	str x0, [sp, #-8]!
	
	// elr_el1 is the user program’s program counter
	// mrs x0, elr_el1  // Move the value of elr_el1 into x0 // not saving this in current build
	// str x0, [sp, #-8]!  // Store x0 on the stack and decrement the stack pointer
	// spsr_el1 is the user program processor state
	// mrs x0, spsr_el1   // Move the value of spsr_el1 into x0
	// str x0, [sp, #-8]!  // Store x0 on the stack and decrement the stack pointer
	// not saving this in current build
	
	// now I need to save the value of sp to a variable
	mov x0, sp // check the current stack pointer, used for debugging
	bl Handle
	
	.balign 0x80
	
	el1_sp0_irq:
	add x0, x0, x0 // Dummy Code
	
	.balign 0x80
	
	el1_sp0_fiq:
	add x0, x0, x0 // Dummy Code
	
	.balign 0x80
	
	el1_sp0_serror:
	add x0, x0, x0 // Dummy Code
	
	.balign 0x80
	
	el1_spx_s:
	b . // EL1 synchronous fault — halt

	.balign 0x80

	el1_spx_irq:
	add x0, x0, x0 // Dummy Code
	
	.balign 0x80
	
	el1_spx_fiq:
	add x0, x0, x0 // Dummy Code
	
	.balign 0x80
	
	el1_spx_serror:
	add x0, x0, x0 // Dummy Code
	
	.balign 0x80
	
	el1_a64_s:
	KERNEL_DISPATCH Handle
	
	.balign 0x80
	
	el1_a64_irq:
	KERNEL_DISPATCH HandleASYNC


	.balign 0x80
	
	el1_a64_fiq:
	add x0, x0, x0 // Dummy Code
	
	.balign 0x80
	
	el1_a64_serror:
	add x0, x0, x0 // Dummy Code
	
	.balign 0x80
	
	el1_a32_s:
	// msr SPSel, #0 // We use the Kernel Stack Pointer
	stp x29,  x30,  [sp, #-16]!
	stp x27,  x28,  [sp, #-16]!
	stp x25,  x26,  [sp, #-16]!
	stp x23,  x24,  [sp, #-16]!
	stp x21,  x22,  [sp, #-16]!
	stp x19, x20, [sp, #-16]!
	stp x17, x18, [sp, #-16]!
	stp x15, x16, [sp, #-16]!
	stp x13, x14, [sp, #-16]!
	stp x11, x12, [sp, #-16]!
	stp x9, x10, [sp, #-16]!
	stp x7, x8, [sp, #-16]!
	stp x5, x6, [sp, #-16]!
	stp x3, x4, [sp, #-16]!
	stp x1, x2, [sp, #-16]!
	str x0, [sp, #-8]!
	
	// elr_el1 is the user program’s program counter
	// mrs x0, elr_el1  // Move the value of elr_el1 into x0 // not saving this in current build
	// str x0, [sp, #-8]!  // Store x0 on the stack and decrement the stack pointer
	// spsr_el1 is the user program processor state
	// mrs x0, spsr_el1   // Move the value of spsr_el1 into x0
	// str x0, [sp, #-8]!  // Store x0 on the stack and decrement the stack pointer
	// not saving this in current build
	
	// now I need to save the value of sp to a variable
	mov x0, sp // check the current stack pointer, used for debugging
	bl Handle
	
	.balign 0x80
	
	el1_a32_irq:
	add x0, x0, x0 // Dummy Code
	
	.balign 0x80
	
	el1_a32_fiq:
	add x0, x0, x0 // Dummy Code
	
	.balign 0x80
	
	el1_a32_serror:
	add x0, x0, x0 // Dummy Code
	
	.balign 0x80

