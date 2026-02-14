/*
 * Layer 0: Syscall stubs (SVC). All SVC invocations used by Layer 1/2 are here.
 * Args and return value follow AAPCS (x0–x4 in, x0 out). Each stub: svc #N; ret.
 */

.text
.align 4

.macro SVC_STUB num
.global asm_svc_\num
asm_svc_\num:
	svc #\num
	ret
.endm

SVC_STUB 0
SVC_STUB 1
SVC_STUB 2
SVC_STUB 3
SVC_STUB 4
SVC_STUB 5
SVC_STUB 6
SVC_STUB 7
SVC_STUB 8
SVC_STUB 9
SVC_STUB 10
SVC_STUB 11
SVC_STUB 12
SVC_STUB 13
SVC_STUB 14
