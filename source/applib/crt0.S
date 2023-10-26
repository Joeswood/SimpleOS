/**
 * Process init - Assembly
 *
 */
     .text
    .global _start
    .extern cstart
_start:
    # reset all the selector
    mov %ss, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    # enter into the main func
    jmp cstart
