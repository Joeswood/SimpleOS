/**
 * Initization of the kernel
 */
 #include "os_cfg.h"

  	//default 32 bits code
 	.text
 	.global _start
	.extern kernel_init
	.extern init_main
_start:
    push 4(%esp)

    # kernel_init(boot_info)
    call kernel_init

	// reload GDT
	jmp $KERNEL_SELECTOR_CS, $gdt_reload

gdt_reload:
	mov $KERNEL_SELECTOR_DS, %ax	
	mov %ax, %ds
    mov %ax, %ss
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

	// set the stack
	mov $(stack + KERNEL_STACK_SIZE), %esp

	// same stack and segament config
	jmp init_main

	.bss
.comm stack, KERNEL_STACK_SIZE    


// when interuption happens, it will switch to stack in PL0, and store ss, esp, cs, eip, flags regs
// need store other regs

	// use mcrio to implement
	// tricky code, can't be debugged. but it's neccessary
   .text
.macro exception_handler name num with_error_code
	    .extern do_handler_\name
		.global exception_handler_\name
	exception_handler_\name:
		// if no error code
		.if \with_error_code == 0
			push $0
		.endif

		// push exception number
		push $\num

		// store all regs
		pushal
		push %ds
		push %es
		push %fs
		push %gs

		// call interupt handler func
		push %esp
		call do_handler_\name
		add $(1*4), %esp		// throw esp

		// restore regs
		pop %gs
		pop %fs
		pop %es
		pop %ds		popal

		// no push exception number and error code
		add $(2*4), %esp
		iret
.endm

exception_handler unknown, -1, 0
exception_handler divider, 0, 0
exception_handler Debug, 1, 0
exception_handler NMI, 2, 0
exception_handler breakpoint, 3, 0
exception_handler overflow, 4, 0
exception_handler bound_range, 5, 0
exception_handler invalid_opcode, 6, 0
exception_handler device_unavailable, 7, 0
exception_handler double_fault, 8, 1
exception_handler invalid_tss, 10, 1
exception_handler segment_not_present, 11, 1
exception_handler stack_segment_fault, 12, 1
exception_handler general_protection, 13, 1
exception_handler page_fault, 14, 1
exception_handler fpu_error, 16, 0
exception_handler alignment_check, 17, 1
exception_handler machine_check, 18, 0
exception_handler smd_exception, 19, 0
exception_handler virtual_exception, 20, 0

// hardware interupt
exception_handler timer, 0x20, 0
exception_handler kbd, 0x21, 0
exception_handler ide_primary, 0x2E, 0
	.text
	.global simple_switch
simple_switch:
	movl 4(%esp), %eax   // from->stack
	movl 8(%esp), %edx   // to->stack

	// store previous task status
	push %ebp
	push %ebx
	push %esi
	push %edi

	// switch stack
	mov %esp, (%eax)    // from->stack = esp
  	mov %edx, %esp      // esp = to->stack

	// load next task stack
	pop %edi
	pop %esi
	pop %ebx
	pop %ebp
  	ret

     .global exception_handler_syscall
    .extern do_handler_syscall
exception_handler_syscall:
	// store previous task status
	pusha
	push %ds
	push %es
	push %fs
	push %gs
	pushf

	// use kernel regs
	mov $(KERNEL_SELECTOR_DS), %eax
	mov %eax, %ds
	mov %eax, %es
	mov %eax, %fs
	mov %eax, %gs

	// call handler func
    mov %esp, %eax
    push %eax
	call do_handler_syscall
	add $4, %esp

	// switch back
	popf
	pop %gs
	pop %fs
	pop %es
	pop %ds
	popa
	
    retf $(5*4)   