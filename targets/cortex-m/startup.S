.syntax unified
.cpu cortex-m3
.thumb

.section .isr_vector, "a", %progbits
.global __vector_table
__vector_table:
  .word _stack_top
  .word Reset_Handler + 1
  .word Default_Handler + 1
  .word Default_Handler + 1
  .word Default_Handler + 1
  .word Default_Handler + 1
  .word Default_Handler + 1
  .word 0
  .word 0
  .word 0
  .word 0
  .word Default_Handler + 1
  .word Default_Handler + 1
  .word 0
  .word Default_Handler + 1
  .word Default_Handler + 1

.text
.thumb_func
.global Reset_Handler
Reset_Handler:
  cpsid i
  ldr sp, =_stack_top
  bl seed_main

.thumb_func
Default_Handler:
  b .
