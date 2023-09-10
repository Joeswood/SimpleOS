/**
 * Boot part. Load the boot by hardware, then load the second loader
 * The boot is stored in the first sector in disk, and we load it into 0x7c00
 */
	#include "boot.h"

  	// 16 bits code
  	.code16
 	.text
	.global _start
	.extern boot_entry
_start:
	// reset seg register
	mov $0, %ax
	mov %ax, %ds
	mov %ax, %ss
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs

	// https://wiki.osdev.org/Memory_Map_(x86)
	// the address before 0x7c00 is used for stack, 30KB for boot and loader
	mov $_start, %esp

	// show message when finish boot
	mov $0xe, %ah
	mov $'L', %al
	int $0x10

	// load loader
	// https://wiki.osdev.org/Disk_access_using_the_BIOS_(INT_13h)
read_loader:
	mov $0x8000, %bx	// address read
	mov $0x2, %cx		// ch:track ，cl first sector 
	mov $0x2, %ah		// ah: 0x2 read disk instr
	mov $64, %al		// al: number of sector read
	mov $0x0080, %dx	// dh: head number，dl driver number 0x80(disk1)
	int $0x13
	jc read_loader

	// jump into c part
	jmp boot_entry

	jmp .

	.section boot_end, "ax"
boot_sig: .byte 0x55, 0xaa
