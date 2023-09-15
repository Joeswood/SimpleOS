/**
 *
 *
 * Two level loader. Initization and kernel loading.
 *
 */
  	// 16 bits code
  	.code16
 	.text
 	.extern loader_entry
	.global _start
_start:
	jmp loader_entry

	// 32 bits code
	.code32
	.text
	.global protect_mode_entry
	.extern load_kernel
protect_mode_entry:
	// reset all the data descriptor
	mov $16, %ax		// data seg
	mov %ax, %ds
    mov %ax, %ss
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

	// long jump to 32 bits kernel
    jmp $8, $load_kernel


