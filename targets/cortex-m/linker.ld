ENTRY(Reset_Handler)

MEMORY
{
  SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00008000
}

SECTIONS
{
  . = ORIGIN(SRAM);

  .isr_vector : ALIGN(4)
  {
    KEEP(*(.isr_vector))
  } > SRAM

  .patch_table : ALIGN(4)
  {
    KEEP(*(.patch_table))
  } > SRAM

  .text : ALIGN(4)
  {
    *(.text*)
    *(.rodata*)
  } > SRAM

  .data : ALIGN(4)
  {
    *(.data*)
  } > SRAM

  .bss (NOLOAD) : ALIGN(4)
  {
    *(.bss*)
    *(COMMON)
  } > SRAM

  . = ALIGN(8);
  PROVIDE(_stack_top = ORIGIN(SRAM) + LENGTH(SRAM));
}
