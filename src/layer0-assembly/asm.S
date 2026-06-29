#include "config.h"

.text
.align 4
.global	Begin
Begin:
	// x0 = Register buffer pointer, x1 = PC, x2 = Stack Pointer, x3 = PSTATE
	// Set up exception return context
	msr elr_el1, x1
	msr sp_el0, x2
	msr spsr_el1, x3

	// Reset the EL1 kernel stack pointer (cpu-0 slot).
	ldr x10, =__cpu_stack_end
	mov sp, x10

	// x0 = register buffer pointer — load x1–x30 via fixed offsets from x0.
	ldp x1, x2, [x0, #8]
	ldp x3, x4, [x0, #24]
	ldp x5, x6, [x0, #40]
	ldp x7, x8, [x0, #56]
	ldp x9, x10, [x0, #72]
	ldp x11, x12, [x0, #88]
	ldp x13, x14, [x0, #104]
	ldp x15, x16, [x0, #120]
	ldp x17, x18, [x0, #136]
	ldp x19, x20, [x0, #152]
	ldp x21, x22, [x0, #168]
	ldp x23, x24, [x0, #184]
	ldp x25, x26, [x0, #200]
	ldp x27, x28, [x0, #216]
	ldp x29, x30, [x0, #232]
	
	// Finally load X0 (now it's safe since we don't need the pointer anymore)
	ldr x0, [x0]

	eret

.global Save
Save:
	// At this point, there should be important values on the stack, as well as stuff on the
	// registers being passed in
	// since the C function and the compiler may have been mean to me, I am going to now reset the stack pointer
	mov sp, x0 // The stack pointer now should be where we left off
	// x1 Registers, x2 PC pointer, x3 Stack Pointer, x4 PSTATE
	// x0 is now free to use
	mrs x0, elr_el1  // Move the value of elr_el1 into x0 // elr_el1 is the pc
	str x0, [x2], 0  // Store x0
	mrs x0, sp_el0  // Move the value of sp_el0 into x0 // sp_el0 is the sp
	str x0, [x3], 0  // Store x0
	mrs x0, spsr_el1 // This one is the PSTATE
	str w0, [x4], 0 // using code w for 32 bit because pstate is only 32 bits. Using 64 would instead write the extra data to parentpid.
	// Now, x2, x3, x4 are free
	// We store the registers off the stack into [x1]
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8 // 10 saved
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8 // 20 saved
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8 // 30 saved
	ldr x2, [sp], #8
	str x2, [x1], 8 // All 31 saved!!
	
	mrs x0, ESR_EL1 // Now we pass the exception code to the next whatever
	ret
	// bl Exception

.global ASYNCSave
ASYNCSave:
	// At this point, there should be important values on the stack, as well as stuff on the
	// registers being passed in
	// since the C function and the compiler may have been mean to me, I am going to now reset the stack pointer
	mov sp, x0 // The stack pointer now should be where we left off
	// x1 Registers, x2 PC pointer, x3 Stack Pointer, x4 PSTATE
	// x0 is now free to use
	mrs x0, elr_el1  // Move the value of elr_el1 into x0 // elr_el1 is the pc
	str x0, [x2], 0  // Store x0
	mrs x0, sp_el0  // Move the value of sp_el0 into x0 // sp_el0 is the sp
	str x0, [x3], 0  // Store x0
	mrs x0, spsr_el1 // This one is the PSTATE
	str w0, [x4], 0 // using code w for 32 bit because pstate is only 32 bits. Using 64 would instead write the extra data to parentpid.
	// Now, x2, x3, x4 are free
	// We store the registers off the stack into [x1]
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8 // 10 saved
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8 // 20 saved
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8
	ldp x2, x3, [sp], #16
	str x2, [x1], 8
	str x3, [x1], 8 // 30 saved
	ldr x2, [sp], #8
	str x2, [x1], 8 // All 31 saved!!
	
	mrs x0, ESR_EL1 // Now we pass the exception code to the next whatever
	bl ExceptionASYNC

// .global save_stack_pointer
// generate the code to switch the task. BY switching the task I mean setting the value of the stack pointer at task level to the address of the 
// this instruction is to be executed in the kernel mode. 
// the function takes in one variable which is the location of the stackpointer metadata in the program.
// such is stored at memory location contained in the register x0
// save_stack_pointer: // I don't think that this code is needed // But keeping it anyways - R
// 	// x0: the memory address of void *stackpointer;
// 	msr SPSel, #0
// 	str sp, [x0] // store sp into the memory address in x0

.global push_trap_frame
push_trap_frame:
	// Adding X0 - X30 onto the user stack.
	// Register x30 is actually the first one that is saved
	msr SPSel, #0
	stp x0,  x1,  [sp, #-16]!
	stp x2,  x3,  [sp, #-16]!
	stp x4,  x5,  [sp, #-16]!
	stp x6,  x7,  [sp, #-16]!
	stp x8,  x9,  [sp, #-16]!
	stp x10, x11, [sp, #-16]!
	stp x12, x13, [sp, #-16]!
	stp x14, x15, [sp, #-16]!
	stp x16, x17, [sp, #-16]!
	stp x18, x19, [sp, #-16]!
	stp x20, x21, [sp, #-16]!
	stp x22, x23, [sp, #-16]!
	stp x24, x25, [sp, #-16]!
	stp x26, x27, [sp, #-16]!
	stp x28, x29, [sp, #-16]!
	str x30, [sp, #-8]! // x30 should already be saved at this point
	// elr_el1 is the user program’s program counter
	mrs x0, elr_el1  // Move the value of elr_el1 into x0
	str x0, [sp, #-8]!  // Store x0 on the stack and decrement the stack pointer
	// spsr_el1 is the user program processor state
	mrs x0, spsr_el1   // Move the value of spsr_el1 into x0
	str x0, [sp, #-8]!  // Store x0 on the stack and decrement the stack pointer
	
	// now I need to save the value of sp to a variable

    ret               // Return from function


.global pop_trap_frame
pop_trap_frame:
	// ideally, sp should be in 
	msr SPSel, #0
	// restoring the register spsr_el1
	// MRS x0, sp_el0  // Move sp into x0
	LDR x1, [sp], #8  // Load value from address in x0 into x1, then increment x0 by 8
	MSR spsr_el1, x1  // Move x1 into spsr_el1
	// MSR sp_el0, x0  // Move the incremented value of x0 back into sp
	// restoring the register elr_el1 
	// MRS x0, sp_el0  // Move sp into x0
	LDR x1, [sp], #8  // Load value from address in x0 into x1, then increment x0 by 8
	MSR elr_el1, x1  // Move x1 into spsr_el1
	// MSR sp_el0, x0  // Move the incremented value of x0 back into sp
	// restoring the register X0-X30
	
	ldp x29, x30, [sp], #16
	ldp x27, x28, [sp], #16
	ldp x25, x26, [sp], #16
	ldp x23, x24, [sp], #16
	ldp x21, x22, [sp], #16
	ldp x19, x20, [sp], #16
	ldp x17, x18, [sp], #16
	ldp x15, x16, [sp], #16
	ldp x13, x14, [sp], #16
	ldp x11, x12, [sp], #16
	ldp x9,  x10, [sp], #16
	ldp x7,  x8,  [sp], #16
	ldp x5,  x6,  [sp], #16
	ldp x3,  x4,  [sp], #16
	ldp x1,  x2,  [sp], #16
	ldr x0, [sp], #8

    ret               // Return from function

.global wfi
wfi:
	WFI // puts the CPU into WFI mode
	ret

.global wfe
wfe:
	WFE
	ret

.global sevl
sevl:
	SEVL
	ret