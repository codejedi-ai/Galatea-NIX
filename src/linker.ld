ENTRY(_start)

/* Separate RX / R / RW segments — avoids ld "LOAD segment with RWX permissions". */
PHDRS
{
  text   PT_LOAD FLAGS(5);  /* PF_R | PF_X */
  rodata PT_LOAD FLAGS(4);  /* PF_R */
  data   PT_LOAD FLAGS(6);  /* PF_R | PF_W */
}

SECTIONS {
  /* Raspberry Pi 4: firmware loads kernel8.img at 0x80000 (arm_64bit=1). */
  . = LINK_BASE;
  __image_start = .;

  /* Code + read-only data */
  .text : {
    KEEP(*(.text.boot))
    *(.text*)
  } :text
  . = ALIGN(0x1000);
  .rodata : {
    *(.rodata*)
  } :rodata
  . = ALIGN(0x1000);
  __code_end = .;            /* page-aligned end of the RO+exec region */

  /* Writable data */
  .data : {
    *(.data*)
  } :data
  . = ALIGN(0x1000);
  .bss (NOLOAD) : {
    __bss_start = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(8);
    __bss_end = .;
  } :data
  . = ALIGN(0x1000);
  __image_end = .;

  /* Per-CPU EL1 stacks (16 KB each) for SMP secondaries. */
  . = ALIGN(16);
  __cpu_stack_base = .;
  . += 0x4000 * 4;
  __cpu_stack_end = .;
}
