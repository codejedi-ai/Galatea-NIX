/*
 * Layer 0: Declarations for assembly functions (spinlock + syscall stubs).
 * All ASM is implemented in layer0-assembly .S files; no inline ASM in C.
 */

#ifndef _LAYER0_H_
#define _LAYER0_H_ 1

#include <stdint.h>

/* ----- Spinlock (ARM64 LDXR/STXR) ----- */
void spinlock_acquire(volatile int *lock);
void spinlock_release(volatile int *lock);

/* ----- Syscall stubs (svc 0..14); args in x0–x4, return in x0 ----- */
void asm_svc_0(void);
void asm_svc_1(void);
int asm_svc_2(uint8_t priority, void (*function)(void));
int asm_svc_3(void);
int asm_svc_4(void);
void asm_svc_5(void);   /* Send: tid,msg,msglen,reply,replylen in x0–x4 */
void asm_svc_6(void);   /* Receive: tid,msg,msglen in x0–x2 */
void asm_svc_7(void);   /* Reply: tid,reply,replylen in x0–x2 */
int asm_svc_8(void);
int asm_svc_9(void);    /* CreateArgs */
int asm_svc_10(void);   /* AwaitEvent */
int asm_svc_11(void);
int asm_svc_12(void);
int asm_svc_13(void);
void *asm_svc_14(void);

/* WFI (already in asm.S) */
void wfi(void);

#endif
