/**
 * Memory Mangement
 */
#include "tools/klib.h"
#include "tools/log.h"
#include "core/memory.h"
#include "tools/klib.h"
#include "cpu/mmu.h"
#include "dev/console.h"

static addr_alloc_t paddr_alloc;        // physical address allocation structure
static pde_t kernel_page_dir[PDE_CNT] __attribute__((aligned(MEM_PAGE_SIZE))); // kernel page dir

/**
 * @brief Retrieve current page table address
 */
static pde_t * current_page_dir (void) {
    return (pde_t *)task_current()->tss.cr3;
}

/**
 * @brief init address allocation structure
 */
static void addr_alloc_init (addr_alloc_t * alloc, uint8_t * bits,
                    uint32_t start, uint32_t size, uint32_t page_size) {
    mutex_init(&alloc->mutex);
    alloc->start = start;
    alloc->size = size;
    alloc->page_size = page_size;
    bitmap_init(&alloc->bitmap, bits, alloc->size / page_size, 0);
}

/**
 * @brief Allocate Multi-Page Memory
 */
static uint32_t addr_alloc_page (addr_alloc_t * alloc, int page_count) {
    uint32_t addr = 0;

    mutex_lock(&alloc->mutex);

    int page_index = bitmap_alloc_nbits(&alloc->bitmap, 0, page_count);
    if (page_index >= 0) {
        addr = alloc->start + page_index * alloc->page_size;
    }

    mutex_unlock(&alloc->mutex);
    return addr;
}

/**
 * @brief Release multi-page memory
 */
static void addr_free_page (addr_alloc_t * alloc, uint32_t addr, int page_count) {
    mutex_lock(&alloc->mutex);

    uint32_t pg_idx = (addr - alloc->start) / alloc->page_size;
    bitmap_set_bit(&alloc->bitmap, pg_idx, page_count, 0);

    mutex_unlock(&alloc->mutex);
}

static void show_mem_info (boot_info_t * boot_info) {
    log_printf("mem region:");
    for (int i = 0; i < boot_info->ram_region_count; i++) {
        log_printf("[%d]: 0x%x - 0x%x", i,
                    boot_info->ram_region_cfg[i].start,
                    boot_info->ram_region_cfg[i].size);
    }
    log_printf("\n");
}

/**
 * @brief Accquire available memory size
 */
static uint32_t total_mem_size(boot_info_t * boot_info) {
    int mem_size = 0;

    // For easy way, don't consider there is a hole in the memory
    for (int i = 0; i < boot_info->ram_region_count; i++) {
        mem_size += boot_info->ram_region_cfg[i].size;
    }
    return mem_size;
}

pte_t * find_pte (pde_t * page_dir, uint32_t vaddr, int alloc) {
    pte_t * page_table;

    pde_t *pde = page_dir + pde_index(vaddr);
    if (pde->present) {
        page_table = (pte_t *)pde_paddr(pde);
    } else {
        // if there is no page table, allocate one
        if (alloc == 0) {
            return (pte_t *)0;
        }

        // allocate a physical page table
        uint32_t pg_paddr = addr_alloc_page(&paddr_alloc, 1);
        if (pg_paddr == 0) {
            return (pte_t *)0;
        }

        // set to user-readable and writable
        pde->v = pg_paddr | PTE_P | PTE_W | PDE_U;

        //kernel_pg_last[pde_index(vaddr)].v = pg_paddr | PTE_P | PTE_W;

        // clear page table in case exception
        page_table = (pte_t *)(pg_paddr);
        kernel_memset(page_table, 0, MEM_PAGE_SIZE);
    }

    return page_table + pte_index(vaddr);
}

/**
 * @brief Map the specified address space with one page
 */
int memory_create_map (pde_t * page_dir, uint32_t vaddr, uint32_t paddr, int count, uint32_t perm) {
    for (int i = 0; i < count; i++) {
        // log_printf("create map: v-0x%x p-0x%x, perm: 0x%x", vaddr, paddr, perm);

        pte_t * pte = find_pte(page_dir, vaddr, 1);
        if (pte == (pte_t *)0) {
            // log_printf("create pte failed. pte == 0");
            return -1;
        }

        ASSERT(pte->present == 0);

        pte->v = paddr | perm | PTE_P;

        vaddr += MEM_PAGE_SIZE;
        paddr += MEM_PAGE_SIZE;
    }

    return 0;
}

/**
 * @brief Based on the memory mapping table, construct the kernel page table.
 */
void create_kernel_table (void) {
    extern uint8_t s_text[], e_text[], s_data[], e_data[];
    extern uint8_t kernel_base[];

    // address mapping table, used to establish kernel-level address mappings
    // addresses remain the same but attributes are added
    static memory_map_t kernel_map[] = {
        {kernel_base,   s_text,         0,              PTE_W},         // kernel stack
        {s_text,        e_text,         s_text,         0},         // kernel code
        {s_data,        (void *)(MEM_EBDA_START - 1),   s_data,        PTE_W},      // kernel data
        {(void *)CONSOLE_DISP_ADDR, (void *)(CONSOLE_DISP_END - 1), (void *)CONSOLE_VIDEO_BASE, PTE_W},

        // expanding the storage space with one-to-one mapping for easy direct manipulation
        {(void *)MEM_EXT_START, (void *)MEM_EXT_END,     (void *)MEM_EXT_START, PTE_W},
    };

    // clear kernel page dir
    kernel_memset(kernel_page_dir, 0, sizeof(kernel_page_dir));

    // after clearing, then create mapping tables one by one based on the mapping relationships
    for (int i = 0; i < sizeof(kernel_map) / sizeof(memory_map_t); i++) {
        memory_map_t * map = kernel_map + i;

        // There may be multiple pages, configuring multiple pages
        // For simplicity, we do not consider the 4MB case
        int vstart = down2((uint32_t)map->vstart, MEM_PAGE_SIZE);
        int vend = up2((uint32_t)map->vend, MEM_PAGE_SIZE);
        int page_count = (vend - vstart) / MEM_PAGE_SIZE;

        memory_create_map(kernel_page_dir, vstart, (uint32_t)map->pstart, page_count, map->perm);
    }
}

/**
 * @brief Create the init page table for process
 * The main task is to create a page directory table and then copy a portion from the kernel page table
 */
uint32_t memory_create_uvm (void) {
    pde_t * page_dir = (pde_t *)addr_alloc_page(&paddr_alloc, 1);
    if (page_dir == 0) {
        return 0;
    }
    kernel_memset((void *)page_dir, 0, MEM_PAGE_SIZE);

    // copy the page directory entries for the entire kernel space to share it with other processes. 
    // memory mapping for user space is not currently handled; it will be created when loading programs
    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);
    for (int i = 0; i < user_pde_start; i++) {
        page_dir[i].v = kernel_page_dir[i].v;
    }

    return (uint32_t)page_dir;
}

/**
 * @brief Destroy user space memory.
 */
void memory_destroy_uvm (uint32_t page_dir) {
    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);
    pde_t * pde = (pde_t *)page_dir + user_pde_start;

    ASSERT(page_dir != 0);

    // eelease the corresponding entries in the page table, excluding the mapped kernel pages.
    for (int i = user_pde_start; i < PDE_CNT; i++, pde++) {
        if (!pde->present) {
            continue;
        }

        // free the physical pages corresponding to the page table and the page table itself.
        pte_t * pte = (pte_t *)pde_paddr(pde);
        for (int j = 0; j < PTE_CNT; j++, pte++) {
            if (!pte->present) {
                continue;
            }

            addr_free_page(&paddr_alloc, pte_paddr(pte), 1);
        }

        addr_free_page(&paddr_alloc, (uint32_t)pde_paddr(pde), 1);
    }

    // page dir table
    addr_free_page(&paddr_alloc, page_dir, 1);
}

/**
 * @brief Copy the page table along with all of its memory space
 */
uint32_t memory_copy_uvm (uint32_t page_dir) {
    // copy the base page table
    uint32_t to_page_dir = memory_create_uvm();
    if (to_page_dir == 0) {
        goto copy_uvm_failed;
    }

    // copy the entries for user space.
    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);
    pde_t * pde = (pde_t *)page_dir + user_pde_start;

    // traverse the page directory entries for user space
    for (int i = user_pde_start; i < PDE_CNT; i++, pde++) {
        if (!pde->present) {
            continue;
        }

        // iterate the page table
        pte_t * pte = (pte_t *)pde_paddr(pde);
        for (int j = 0; j < PTE_CNT; j++, pte++) {
            if (!pte->present) {
                continue;
            }

            // allocate the physical memory
            uint32_t page = addr_alloc_page(&paddr_alloc, 1);
            if (page == 0) {
                goto copy_uvm_failed;
            }

            // build mapping
            uint32_t vaddr = (i << 22) | (j << 12);
            int err = memory_create_map((pde_t *)to_page_dir, vaddr, page, 1, get_pte_perm(pte));
            if (err < 0) {
                goto copy_uvm_failed;
            }

            // copy
            kernel_memcpy((void *)page, (void *)vaddr, MEM_PAGE_SIZE);
        }
    }
    return to_page_dir;

copy_uvm_failed:
    if (to_page_dir) {
        memory_destroy_uvm(to_page_dir);
    }
    return -1;
}

/**
 * @brief Retrieve the physical address of the specified virtual address
 * If fail, return 0
 */
uint32_t memory_get_paddr (uint32_t page_dir, uint32_t vaddr) {
    pte_t * pte = find_pte((pde_t *)page_dir, vaddr, 0);
    if (pte == (pte_t *)0) {
        return 0;
    }

    return pte_paddr(pte) + (vaddr & (MEM_PAGE_SIZE - 1));
}

/**
 * @brief Copy a string between different process spaces
 * page_dir is the target page table, while the current one is still the old page table
 */
int memory_copy_uvm_data(uint32_t to, uint32_t page_dir, uint32_t from, uint32_t size) {
    char *buf, *pa0;

    while(size > 0){
        // Retrieve the target's physical address, which is another virtual address for it
        uint32_t to_paddr = memory_get_paddr(page_dir, to);
        if (to_paddr == 0) {
            return -1;
        }

        // calculate the current available copy size
        uint32_t offset_in_page = to_paddr & (MEM_PAGE_SIZE - 1);
        uint32_t curr_size = MEM_PAGE_SIZE - offset_in_page;
        if (curr_size > size) {
            curr_size = size;       // if it's too large and exceeds a page boundary, only copy within this page
        }

        kernel_memcpy((void *)to_paddr, (void *)from, curr_size);

        size -= curr_size;
        to += curr_size;
        from += curr_size;
  }

  return 0;
}

uint32_t memory_alloc_for_page_dir (uint32_t page_dir, uint32_t vaddr, uint32_t size, int perm) {
    uint32_t curr_vaddr = vaddr;
    int page_count = up2(size, MEM_PAGE_SIZE) / MEM_PAGE_SIZE;
    vaddr = down2(vaddr, MEM_PAGE_SIZE);

    // allocate memory page by page and then establish mapping relationships
    for (int i = 0; i < page_count; i++) {
        uint32_t paddr = addr_alloc_page(&paddr_alloc, 1);
        if (paddr == 0) {
            log_printf("mem alloc failed. no memory");
            return 0;
        }

        // match the memory and specific address
        int err = memory_create_map((pde_t *)page_dir, curr_vaddr, paddr, 1, perm);
        if (err < 0) {
            log_printf("create memory map failed. err = %d", err);
            addr_free_page(&paddr_alloc, vaddr, i + 1);
            return -1;
        }

        curr_vaddr += MEM_PAGE_SIZE;
    }

    return 0;
}

/**
 * @brief Allocate multiple memory pages for the specified virtual address space
 */
int memory_alloc_page_for (uint32_t addr, uint32_t size, int perm) {
    return memory_alloc_for_page_dir(task_current()->tss.cr3, addr, size, perm);
}


/**
 * @brief Allocate ONE page
 * used for memory allocation in the kernel space, not for process memory space
 */
uint32_t memory_alloc_page (void) {
    // In the kernel space, virtual addresses are the same as physical addresses
    return addr_alloc_page(&paddr_alloc, 1);
}

/**
 * @brief Release ONE page
 */
void memory_free_page (uint32_t addr) {
    if (addr < MEMORY_TASK_BASE) {
        // kernel space, release immediately
        addr_free_page(&paddr_alloc, addr, 1);
    } else {
        // process space, release the page table
        pte_t * pte = find_pte(current_page_dir(), addr, 0);
        ASSERT((pte == (pte_t *)0) && pte->present);

        // free memory page
        addr_free_page(&paddr_alloc, pte_paddr(pte), 1);


        pte->v = 0;
    }
}

/**
 * @brief Initialize the memory management system
 * 
 *  1. Initialize the physical memory allocator: Manage all physical memory. Allocate a physical bitmap within the first 1MB of memory.
 *  2. Recreate the kernel page table: The page table created by the original loader is no longer suitable.
 */
void memory_init (boot_info_t * boot_info) {
    // starting from the beginning of the 1MB memory space, as defined in the linker script
    extern uint8_t * mem_free_start;

    log_printf("mem init.");
    show_mem_info(boot_info);

    // place the physical page bitmap behind the kernel data
    uint8_t * mem_free = (uint8_t *)&mem_free_start; 

    // calculate the free memory capacity above 1MB and align it to page boundaries
    uint32_t mem_up1MB_free = total_mem_size(boot_info) - MEM_EXT_START;
    mem_up1MB_free = down2(mem_up1MB_free, MEM_PAGE_SIZE);   // 对齐到4KB页
    log_printf("Free memory: 0x%x, size: 0x%x", MEM_EXT_START, mem_up1MB_free);

    // to manage 4GB of memory, a total of 4 * 1024 * 1024 * 1024 / 4096 / 8 = 128KB of bitmap is needed, which is accommodated in the lower 1MB of RAM space
    addr_alloc_init(&paddr_alloc, mem_free, MEM_EXT_START, mem_up1MB_free, MEM_PAGE_SIZE);
    mem_free += bitmap_byte_count(paddr_alloc.size / MEM_PAGE_SIZE);

    ASSERT(mem_free < (uint8_t *)MEM_EBDA_START);

    // create the kernel page table and switch to it
    create_kernel_table();

    // switch to the current page table
    mmu_set_page_dir((uint32_t)kernel_page_dir);
}

/**
 * @brief Adjust memory allocation for the heap and return the pointer to the heap before the adjustment
 */
char * sys_sbrk(int incr) {
    task_t * task = task_current();
    char * pre_heap_end = (char * )task->heap_end;
    int pre_incr = incr;

    ASSERT(incr >= 0);

    // if the address is 0, return the top of the valid heap area
    if (incr == 0) {
        log_printf("sbrk(0): end = 0x%x", pre_heap_end);
        return pre_heap_end;
    } 
    
    uint32_t start = task->heap_end;
    uint32_t end = start + incr;

    int start_offset = start % MEM_PAGE_SIZE;
    if (start_offset) {
        // no more than one page
        if (start_offset + incr <= MEM_PAGE_SIZE) {
            task->heap_end = end;
            return pre_heap_end;
        } else {
            // more than one page
            uint32_t curr_size = MEM_PAGE_SIZE - start_offset;
            start += curr_size;
            incr -= curr_size;
        }
    }

    // handle the remaining, page-aligned, portions
    if (incr) {
        uint32_t curr_size = end - start;
        int err = memory_alloc_page_for(start, curr_size, PTE_P | PTE_U | PTE_W);
        if (err < 0) {
            log_printf("sbrk: alloc mem failed.");
            return (char *)-1;
        }
    }

    //log_printf("sbrk(%d): end = 0x%x", pre_incr, end);
    task->heap_end = end;
    return (char * )pre_heap_end;        
}