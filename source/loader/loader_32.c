/**
 *
 * The code is run under 32 bits (protected mode)
 * Two level loader
 * 
 */
#include "loader.h"
#include "comm/elf.h"

/**
* Use LBA 48 bits read disk
*/
static void read_disk(int sector, int sector_count, uint8_t * buf) {
    outb(0x1F6, (uint8_t) (0xE0));

	outb(0x1F2, (uint8_t) (sector_count >> 8));
    outb(0x1F3, (uint8_t) (sector >> 24));		// LBA 24-31 bits
    outb(0x1F4, (uint8_t) (0));					// LBA 32-39 bits
    outb(0x1F5, (uint8_t) (0));					// LBA 40-47 bits

    outb(0x1F2, (uint8_t) (sector_count));
	outb(0x1F3, (uint8_t) (sector));			// LBA 0-7 bits
	outb(0x1F4, (uint8_t) (sector >> 8));		// LBA 8-15 bits
	outb(0x1F5, (uint8_t) (sector >> 16));		// LBA 16-23 bits

	outb(0x1F7, (uint8_t) 0x24);

    // read data
	uint16_t *data_buf = (uint16_t*) buf;
	while (sector_count-- > 0) {
        // check the sectors before reading
		while ((inb(0x1F7) & 0x88) != 0x8) {}

        // read data and write into buffer
		for (int i = 0; i < SECTOR_SIZE / 2; i++) {
			*data_buf++ = inw(0x1F0);
		}
	}
}

/**
 * Analyze elf file，get the content into corresponding place
 * Ref: https://wiki.osdev.org/ELF elf format
 * 
 */
static uint32_t reload_elf_file (uint8_t * file_buffer) {
    // extract data and code from elf
    // if there is valid 
    Elf32_Ehdr * elf_hdr = (Elf32_Ehdr *)file_buffer;
    if ((elf_hdr->e_ident[0] != ELF_MAGIC) || (elf_hdr->e_ident[1] != 'E')
        || (elf_hdr->e_ident[2] != 'L') || (elf_hdr->e_ident[3] != 'F')) {
        return 0;
    }

    // load elf header, and copy content to corresponding place
    for (int i = 0; i < elf_hdr->e_phnum; i++) {
        Elf32_Phdr * phdr = (Elf32_Phdr *)(file_buffer + elf_hdr->e_phoff) + i;
        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        // use physical address, paging is not working now
        uint8_t * src = file_buffer + phdr->p_offset;
        uint8_t * dest = (uint8_t *)phdr->p_paddr;
        for (int j = 0; j < phdr->p_filesz; j++) {
            *dest++ = *src++;
        }

		dest= (uint8_t *)phdr->p_paddr + phdr->p_filesz;
		for (int j = 0; j < phdr->p_memsz - phdr->p_filesz; j++) {
			*dest++ = 0;
		}
    }

    return elf_hdr->e_entry;
}

/**
 * Die
 */
static void die (int code) {
    for (;;) {}
}


/**
 * Enable paging
 * Map the 0-4M space to the 0-4M space and the SYS_KERNEL_BASE_ADDR~+4MB space
 * The mapping of 0-4MB is primarily used to ensure that the loader can continue to function properly while protecting itself
 * SYS_KERNEL_BASE_ADDR+4MB is used to provide the correct virtual address space for the kernel
 */
void enable_page_mode (void) {
#define PDE_P			(1 << 0)
#define PDE_PS			(1 << 7)
#define PDE_W			(1 << 1)
#define CR4_PSE		    (1 << 4)
#define CR0_PG		    (1 << 31)

    // use 4MB page entry, easy way
    // temporary use, help to start kernel, after kernel started, reset the page table
    static uint32_t page_dir[1024] __attribute__((aligned(4096))) = {
        [0] = PDE_P | PDE_PS | PDE_W,			// PDE_PS，enable 4MB page
    };

    // set PSE, enable 4MB page, not 4KB
    uint32_t cr4 = read_cr4();
    write_cr4(cr4 | CR4_PSE);

    // set page table address
    write_cr3((uint32_t)page_dir);

    // enable paging
    write_cr0(read_cr0() | CR0_PG);
}

/**
 * load kernel from disk
 */
void load_kernel(void) {
    // read 500 sectors
    read_disk(100, 500, (uint8_t *)SYS_KERNEL_LOAD_ADDR);

     // analyze elf file, enter kernel and pass boot parameters
     // put elf file into SYS_KERNEL_LOAD_ADDR temporarily, then analyze
    uint32_t kernel_entry = reload_elf_file((uint8_t *)SYS_KERNEL_LOAD_ADDR);
	if (kernel_entry == 0) {
		die(-1);
	}

    // enable paging
	enable_page_mode();

    ((void (*)(boot_info_t *))kernel_entry)(&boot_info);
    for (;;) {}
}