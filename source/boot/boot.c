
__asm__(".code16gcc");

#include "boot.h"

#define	LOADER_START_ADDR	0x8000		// address to load loader

/**
 * Boot entry
 * find the loader and jump to loader
 */
void boot_entry(void) {
	((void (*)(void))LOADER_START_ADDR)();
} 

