#ifndef _asm_h_
#define _asm_h_ 1
#include <stdint.h>
// teh framework for calling assembly functions:
// Firstly the registers are the parameters. Parameter_# is corrusponding to the register number
// the return register os x0
void Begin(void*, void (*)(), void*, uint32_t); // Register pointer to first reg
// Reg, PC, STACK, PSTATE
uint64_t Save(void*, void*, void (**)(), void**, uint32_t*);
uint64_t ASYNCSave(void*, void*, void (**)(), void**, uint32_t*);
// stack returnptr, Reg, PC, STACK, PSTATE
void push_trap_frame();
void pop_trap_frame();
void save_stack_pointer(void*);

void wfi();
#endif
