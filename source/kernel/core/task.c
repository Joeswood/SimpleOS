/**
 * Task Management
 */
#include "comm/cpu_instr.h"
#include "core/task.h"
#include "tools/klib.h"
#include "tools/log.h"
#include "os_cfg.h"
#include "cpu/irq.h"
#include "core/memory.h"
#include "cpu/cpu.h"
#include "cpu/mmu.h"
#include "core/syscall.h"
#include "comm/elf.h"
#include "fs/fs.h"

static task_manager_t task_manager;     // Task Manager
static uint32_t idle_task_stack[IDLE_STACK_SIZE];	// idle Task Stack
static task_t task_table[TASK_NR];      // User Process Table
static mutex_t task_table_mutex;        // Process Table Mutex

static int tss_init (task_t * task, int flag, uint32_t entry, uint32_t esp) {
    // assign GDT for TSS
    int tss_sel = gdt_alloc_desc();
    if (tss_sel < 0) {
        log_printf("alloc tss failed.\n");
        return -1;
    }

    segment_desc_set(tss_sel, (uint32_t)&task->tss, sizeof(tss_t),
            SEG_P_PRESENT | SEG_DPL0 | SEG_TYPE_TSS);

    // init TSS segement
    kernel_memset(&task->tss, 0, sizeof(tss_t));

    // allocate kernel stack (physical addr)
    uint32_t kernel_stack = memory_alloc_page();
    if (kernel_stack == 0) {
        goto tss_init_failed;
    }
    
    // select different access selectors based on different permissions
    int code_sel, data_sel;
    if (flag & TASK_FLAG_SYSTEM) {
        code_sel = KERNEL_SELECTOR_CS;
        data_sel = KERNEL_SELECTOR_DS;
    } else {
        // NOTE: must add RP3 for seg protection
        code_sel = task_manager.app_code_sel | SEG_RPL3;
        data_sel = task_manager.app_data_sel | SEG_RPL3;
    }

    task->tss.eip = entry;
    task->tss.esp = esp ? esp : kernel_stack + MEM_PAGE_SIZE;  // Use the kernel stack if no stack is specified, which means running in privilege level 0 process
    task->tss.esp0 = kernel_stack + MEM_PAGE_SIZE;
    task->tss.ss0 = KERNEL_SELECTOR_DS;
    task->tss.eip = entry;
    task->tss.eflags = EFLAGS_DEFAULT| EFLAGS_IF;
    task->tss.es = task->tss.ss = task->tss.ds = task->tss.fs 
            = task->tss.gs = data_sel;   // all utilize the same data seg
    task->tss.cs = code_sel; 
    task->tss.iomap = 0;

    // init page table
    uint32_t page_dir = memory_create_uvm();
    if (page_dir == 0) {
        goto tss_init_failed;
    }
    task->tss.cr3 = page_dir;

    task->tss_sel = tss_sel;
    return 0;
tss_init_failed:
    gdt_free_sel(tss_sel);

    if (kernel_stack) {
        memory_free_page(kernel_stack);
    }
    return -1;
}

/**
 * @brief Init Task
 */
int task_init (task_t *task, const char * name, int flag, uint32_t entry, uint32_t esp) {
    ASSERT(task != (task_t *)0);

    int err = tss_init(task, flag, entry, esp);
    if (err < 0) {
        log_printf("init task failed.\n");
        return err;
    }

    // init Task seg
    kernel_strncpy(task->name, name, TASK_NAME_SIZE);
    task->state = TASK_CREATED;
    task->sleep_ticks = 0;
    task->time_slice = TASK_TIME_SLICE_DEFAULT;
    task->slice_ticks = task->time_slice;
    task->parent = (task_t *)0;
    task->heap_start = 0;
    task->heap_end = 0;
    list_node_init(&task->all_node);
    list_node_init(&task->run_node);
    list_node_init(&task->wait_node);

    // file related
    kernel_memset(task->file_table, 0, sizeof(task->file_table));

    // insert into the ready queue and all task queues
    irq_state_t state = irq_enter_protection();
    task->pid = (uint32_t)task;   // use addr
    list_insert_last(&task_manager.task_list, &task->all_node);
    irq_leave_protection(state);
    return 0;
}

/**
 * @brief Start Task
 */
void task_start(task_t * task) {
    irq_state_t state = irq_enter_protection();
    task_set_ready(task);
    irq_leave_protection(state);
}

/**
 * @brief Uninit
 */
void task_uninit (task_t * task) {
    if (task->tss_sel) {
        gdt_free_sel(task->tss_sel);
    }

    if (task->tss.esp0) {
        memory_free_page(task->tss.esp0 - MEM_PAGE_SIZE);
    }

    if (task->tss.cr3) {
        memory_destroy_uvm(task->tss.cr3);
    }

    kernel_memset(task, 0, sizeof(task_t));
}

void simple_switch (uint32_t ** from, uint32_t * to);

/**
 * @brief Switch to specific Task
 */
void task_switch_from_to (task_t * from, task_t * to) {
     switch_to_tss(to->tss_sel);
    //simple_switch(&from->stack, to->stack);
}

/**
 * @brief Initialization of the Initial Process
 * We didn't load from disk because it requires a file system. Ideally, we wanted 'init' closely tied to the kernel for efficient loading
 * You can compile 'init' with the kernel, adjusting the loading and execution addresses in 'kernel.lds'
 * However, this might mix 'init' with 'newlib' code. Therefore, it's best to keep them separate.
 */
void task_first_init (void) {
    void first_task_entry (void);

    // following represents the physical memory address of the bin file
    extern uint8_t s_first_task[], e_first_task[];

    // allocated space is slightly larger than the actual storage space, with the extra portion reserved for the stack
    uint32_t copy_size = (uint32_t)(e_first_task - s_first_task);
    uint32_t alloc_size = 10 * MEM_PAGE_SIZE;
    ASSERT(copy_size < alloc_size);

    uint32_t first_start = (uint32_t)first_task_entry;

    task_init(&task_manager.first_task, "first task", 0, first_start, first_start + alloc_size);
    task_manager.first_task.heap_start = (uint32_t)e_first_task;  
    task_manager.first_task.heap_end = task_manager.first_task.heap_start;
    task_manager.curr_task = &task_manager.first_task;

    // update page table addr
    mmu_set_page_dir(task_manager.first_task.tss.cr3);

    // allocate one page of memory for code storage and then copy the code there
    memory_alloc_page_for(first_start,  alloc_size, PTE_P | PTE_W | PTE_U);
    kernel_memcpy((void *)first_start, (void *)&s_first_task, copy_size);

    // start process
    task_start(&task_manager.first_task);

    // write to the TR register to indicate the first task currently running
    write_tr(task_manager.first_task.tss_sel);
}

/**
 * @brief Return init Task
 */
task_t * task_first_task (void) {
    return &task_manager.first_task;
}

/**
 * @brief Idle Task
 */
static void idle_task_entry (void) {
    for (;;) {
        hlt();
    }
}

/**
 * @brief Task Manager Init
 */
void task_manager_init (void) {
    kernel_memset(task_table, 0, sizeof(task_table));
    mutex_init(&task_table_mutex);

    // data and code segments, using DPL3, shared by all applications
    // for debugging convenience, temporarily using DPL0
    int sel = gdt_alloc_desc();
    segment_desc_set(sel, 0x00000000, 0xFFFFFFFF,
                     SEG_P_PRESENT | SEG_DPL3 | SEG_S_NORMAL |
                     SEG_TYPE_DATA | SEG_TYPE_RW | SEG_D);
    task_manager.app_data_sel = sel;

    sel = gdt_alloc_desc();
    segment_desc_set(sel, 0x00000000, 0xFFFFFFFF,
                     SEG_P_PRESENT | SEG_DPL3 | SEG_S_NORMAL |
                     SEG_TYPE_CODE | SEG_TYPE_RW | SEG_D);
    task_manager.app_code_sel = sel;

    // list init
    list_init(&task_manager.ready_list);
    list_init(&task_manager.task_list);
    list_init(&task_manager.sleep_list);

    // idle Task init
    task_init(&task_manager.idle_task,
                "idle task",
                TASK_FLAG_SYSTEM,
                (uint32_t)idle_task_entry,
                0);     // run in kernel mode, PL3 (lowest)
    task_manager.curr_task = (task_t *)0;
    task_start(&task_manager.idle_task);
}

/**
 * @brief Insert Task into ready list
 */
void task_set_ready(task_t *task) {
    if (task != &task_manager.idle_task) {
        list_insert_last(&task_manager.ready_list, &task->run_node);
        task->state = TASK_READY;
    }
}

/**
 * @brief Remove Task from ready list
 */
void task_set_block (task_t *task) {
    if (task != &task_manager.idle_task) {
        list_remove(&task_manager.ready_list, &task->run_node);
    }
}
/**
 * @brief Get next Task
 */
static task_t * task_next_run (void) {
    // if there are no tasks, run the idle task
    if (list_count(&task_manager.ready_list) == 0) {
        return &task_manager.idle_task;
    }
    
    // normal Task
    list_node_t * task_node = list_first(&task_manager.ready_list);
    return list_node_parent(task_node, task_t, run_node);
}

/**
 * @brief Add Task into sleep status
 */
void task_set_sleep(task_t *task, uint32_t ticks) {
    if (ticks <= 0) {
        return;
    }

    task->sleep_ticks = ticks;
    task->state = TASK_SLEEP;
    list_insert_last(&task_manager.sleep_list, &task->run_node);
}

/**
 * @brief Remove Task from waitting list
 * 
 * @param task 
 */
void task_set_wakeup (task_t *task) {
    list_remove(&task_manager.sleep_list, &task->run_node);
}

/**
 * @brief Get current running Task
 */
task_t * task_current (void) {
    return task_manager.curr_task;
}

/**
 * @brief Retrieve file descriptor of current process
 */
file_t * task_file (int fd) {
    if ((fd >= 0) && (fd < TASK_OFILE_NR)) {
        file_t * file = task_current()->file_table[fd];
        return file;
    }

    return (file_t *)0;
}

/**
 * @brief Allocate a new file ID for the specified file
 */
int task_alloc_fd (file_t * file) {
    task_t * task = task_current();

    for (int i = 0; i < TASK_OFILE_NR; i++) {
        file_t * p = task->file_table[i];
        if (p == (file_t *)0) {
            task->file_table[i] = file;
            return i;
        }
    }

    return -1;
}

/**
 * @brief Remove the opened file with file descriptor fd from the task
 */
void task_remove_fd (int fd) {
    if ((fd >= 0) && (fd < TASK_OFILE_NR)) {
        task_current()->file_table[fd] = (file_t *)0;
    }
}

/**
 * @brief Let current Task yield CPU
 */
int sys_yield (void) {
    irq_state_t state = irq_enter_protection();

    if (list_count(&task_manager.ready_list) > 1) {
        task_t * curr_task = task_current();

        // if there are other tasks in the list, move the current task to the end of the list
        task_set_block(curr_task);
        task_set_ready(curr_task);

        // switch to the next task. It's important to protect this section until the switch is complete;
        // otherwise, issues may arise if the next task gets blocked or deleted for some reason after running, and we come back here to switch
        task_dispatch();
    }
    irq_leave_protection(state);

    return 0;
}

/**
 * @brief Execute a task scheduling
 */
void task_dispatch (void) {
    task_t * to = task_next_run();
    if (to != task_manager.curr_task) {
        task_t * from = task_manager.curr_task;

        task_manager.curr_task = to;
        task_switch_from_to(from, to);
    }
}

/**
 * @brief Time handling
 * being called in the interupt handler func
 */
void task_time_tick (void) {
    task_t * curr_task = task_current();

    // handling of time slices
    irq_state_t state = irq_enter_protection();
    if (--curr_task->slice_ticks == 0) {
    // time slice is exhausted, reload the time slice
    // for idle tasks, subtract unused time here
        curr_task->slice_ticks = curr_task->time_slice;

        // adjust the position of the list to the tail
        task_set_block(curr_task);
        task_set_ready(curr_task);
    }
    
    // sleep handling
    list_node_t * curr = list_first(&task_manager.sleep_list);
    while (curr) {
        list_node_t * next = list_node_next(curr);

        task_t * task = list_node_parent(curr, task_t, run_node);
        if (--task->sleep_ticks == 0) {
            // when the delay time expires, remove it from the sleep list and move it to the ready list
            task_set_wakeup(task);
            task_set_ready(task);
        }
        curr = next;
    }

    task_dispatch();
    irq_leave_protection(state);
}

/**
 * @brief Assign with a Task structure
 */
static task_t * alloc_task (void) {
    task_t * task = (task_t *)0;

    mutex_lock(&task_table_mutex);
    for (int i = 0; i < TASK_NR; i++) {
        task_t * curr = task_table + i;
        if (curr->name[0] == 0) {
            task = curr;
            break;
        }
    }
    mutex_unlock(&task_table_mutex);

    return task;
}

/**
 * @brief Release Task structure
 */
static void free_task (task_t * task) {
    mutex_lock(&task_table_mutex);
    task->name[0] = 0;
    mutex_unlock(&task_table_mutex);
}

/**
 * @brief Task enters a sleep state
 */
void sys_msleep (uint32_t ms) {
    // at least delay 1 tick
    if (ms < OS_TICK_MS) {
        ms = OS_TICK_MS;
    }

    irq_state_t state = irq_enter_protection();

    // remove from the ready list and add to the sleep list
    task_set_block(task_manager.curr_task);
    task_set_sleep(task_manager.curr_task, (ms + (OS_TICK_MS - 1))/ OS_TICK_MS);
    
    // execute a scheduling
    task_dispatch();

    irq_leave_protection(state);
}


/**
 * @brief Copy the list of open files from the current process
 */
static void copy_opened_files(task_t * child_task) {
    task_t * parent = task_current();

    for (int i = 0; i < TASK_OFILE_NR; i++) {
        file_t * file = parent->file_table[i];
        if (file) {
            file_inc_ref(file);
            child_task->file_table[i] = parent->file_table[i];
        }
    }
}

/**
 * @brief Create a copy of the process
 */
int sys_fork (void) {
    task_t * parent_task = task_current();

    // assign Task structure
    task_t * child_task = alloc_task();
    if (child_task == (task_t *)0) {
        goto fork_failed;
    }

    syscall_frame_t * frame = (syscall_frame_t *)(parent_task->tss.esp0 - sizeof(syscall_frame_t));

    // initialize the child process and adjust necessary fields.
    // the ESP needs to be reduced by the total number of argument bytes for system calls,
    // as it returns through a normal 'ret' instruction without going through the system call handling 'ret' (which returns the number of arguments).
    int err = task_init(child_task,  parent_task->name, 0, frame->eip,
                        frame->esp + sizeof(uint32_t)*SYSCALL_PARAM_COUNT);
    if (err < 0) {
        goto fork_failed;
    }

    // copy opened file
    copy_opened_files(child_task);

    // retrieve partial state from the parent process's stack and then write it to the TSS
    // check if ESP, EIP, and other values are within the user space range to avoid causing a page fault
    tss_t * tss = &child_task->tss;
    tss->eax = 0;                       // return 0, if child process
    tss->ebx = frame->ebx;
    tss->ecx = frame->ecx;
    tss->edx = frame->edx;
    tss->esi = frame->esi;
    tss->edi = frame->edi;
    tss->ebp = frame->ebp;

    tss->cs = frame->cs;
    tss->ds = frame->ds;
    tss->es = frame->es;
    tss->fs = frame->fs;
    tss->gs = frame->gs;
    tss->eflags = frame->eflags;

    child_task->parent = parent_task;

    // copy the memory space of the parent process to the child process.
    if ((child_task->tss.cr3 = memory_copy_uvm(parent_task->tss.cr3)) < 0) {
        goto fork_failed;
    }

    // after successfully created, return the child pid
    task_start(child_task);
    return child_task->pid;
fork_failed:
    if (child_task) {
        task_uninit (child_task);
        free_task(child_task);
    }
    return -1;
}

/**
 * @brief Load data from a program header into memory
 */
static int load_phdr(int file, Elf32_Phdr * phdr, uint32_t page_dir) {
    // generated ELF file requires page boundary alignment
    ASSERT((phdr->p_vaddr & (MEM_PAGE_SIZE - 1)) == 0);

    // allocate space
    int err = memory_alloc_for_page_dir(page_dir, phdr->p_vaddr, phdr->p_memsz, PTE_P | PTE_U | PTE_W);
    if (err < 0) {
        log_printf("no memory");
        return -1;
    }

    // modify current writing/reading position
    if (sys_lseek(file, phdr->p_offset, 0) < 0) {
        log_printf("read file failed");
        return -1;
    }

    // allocate all memory space for segments. If subsequent operations fail, it will be released at the higher level
    // for simplicity, set it to writable mode. Perhaps consider setting it to read-only based on phdr->flags
    // because the detailed definition of this value was not found, it has not been included
    uint32_t vaddr = phdr->p_vaddr;
    uint32_t size = phdr->p_filesz;
    while (size > 0) {
        int curr_size = (size > MEM_PAGE_SIZE) ? MEM_PAGE_SIZE : size;

        uint32_t paddr = memory_get_paddr(page_dir, vaddr);

        // NOTE: the page used here is current, not other
        if (sys_read(file, (char *)paddr, curr_size) <  curr_size) {
            log_printf("read file failed");
            return -1;
        }

        size -= curr_size;
        vaddr += curr_size;
    }

    return 0;
}

/**
 * @brief Load ELF file into memory
 */
static uint32_t load_elf_file (task_t * task, const char * name, uint32_t page_dir) {
    Elf32_Ehdr elf_hdr;
    Elf32_Phdr elf_phdr;

    // open in only-read way
    int file = sys_open(name, 0);   // TODO:  use 0 tempararily
    if (file < 0) {
        log_printf("open file failed.%s", name);
        goto load_failed;
    }

    // read file header first
    int cnt = sys_read(file, (char *)&elf_hdr, sizeof(Elf32_Ehdr));
    if (cnt < sizeof(Elf32_Ehdr)) {
        log_printf("elf hdr too small. size=%d", cnt);
        goto load_failed;
    }

    // check
    if ((elf_hdr.e_ident[0] != ELF_MAGIC) || (elf_hdr.e_ident[1] != 'E')
        || (elf_hdr.e_ident[2] != 'L') || (elf_hdr.e_ident[3] != 'F')) {
        log_printf("check elf indent failed.");
        goto load_failed;
    }

    // it must be an executable file, of type for the 386 processor, and have an entry point
    if ((elf_hdr.e_type != ET_EXEC) || (elf_hdr.e_machine != ET_386) || (elf_hdr.e_entry == 0)) {
        log_printf("check elf type or entry failed.");
        goto load_failed;
    }

    // it must have program header
    if ((elf_hdr.e_phentsize == 0) || (elf_hdr.e_phoff == 0)) {
        log_printf("none programe header");
        goto load_failed;
    }

    // load the program headers and copy their contents to the corresponding locations
    uint32_t e_phoff = elf_hdr.e_phoff;
    for (int i = 0; i < elf_hdr.e_phnum; i++, e_phoff += elf_hdr.e_phentsize) {
        if (sys_lseek(file, e_phoff, 0) < 0) {
            log_printf("read file failed");
            goto load_failed;
        }

        // parse program header after reading it
        cnt = sys_read(file, (char *)&elf_phdr, sizeof(Elf32_Phdr));
        if (cnt < sizeof(Elf32_Phdr)) {
            log_printf("read file failed");
            goto load_failed;
        }

        // checks
        if ((elf_phdr.p_type != PT_LOAD) || (elf_phdr.p_vaddr < MEMORY_TASK_BASE)) {
           continue;
        }

        // load current program header
        int err = load_phdr(file, &elf_phdr, page_dir);
        if (err < 0) {
            log_printf("load program hdr failed");
            goto load_failed;
        }

        // TODO: more checks here
        task->heap_start = elf_phdr.p_vaddr + elf_phdr.p_memsz;
        task->heap_end = task->heap_start;
   }

    sys_close(file);
    return elf_hdr.e_entry;

load_failed:
    if (file >= 0) {
        sys_close(file);
    }

    return 0;
}

/**
 * @brief Copy process arguments to the stack
 */
static int copy_args (char * to, uint32_t page_dir, int argc, char **argv) {
    // write argc, argv pointers, and parameter strings sequentially into stack top
    task_args_t task_args;
    task_args.argc = argc;
    task_args.argv = (char **)(to + sizeof(task_args_t));

    // copy various parameters, skipping task_args and the argument table, and write each argv parameter into memory space
    char * dest_arg = to + sizeof(task_args_t) + sizeof(char *) * (argc + 1);   
    
    // argv table
    char ** dest_argv_tb = (char **)memory_get_paddr(page_dir, (uint32_t)(to + sizeof(task_args_t)));
    ASSERT(dest_argv_tb != 0);

    for (int i = 0; i < argc; i++) {
        char * from = argv[i];

        int len = kernel_strlen(from) + 1;   
        int err = memory_copy_uvm_data((uint32_t)dest_arg, page_dir, (uint32_t)from, len);
        ASSERT(err >= 0);

        // link with ar
        dest_argv_tb[i] = dest_arg;

        // record current location, move forward
        dest_arg += len;
    }

    // maybe no parameter, no need to write
    if (argc) {
        dest_argv_tb[argc] = '\0';
    }

     // write task_args
    return memory_copy_uvm_data((uint32_t)to, page_dir, (uint32_t)&task_args, sizeof(task_args_t));
}

/**
 * @brief Load a process
 */
int sys_execve(char *name, char **argv, char **env) {
    task_t * task = task_current();

    // page tables will be switched later, so let's handle the cases where data needs to be fetched from the process space first
    kernel_strncpy(task->name, get_file_name(name), TASK_NAME_SIZE);

    // switch to new page table
    uint32_t old_page_dir = task->tss.cr3;
    uint32_t new_page_dir = memory_create_uvm();
    if (!new_page_dir) {
        goto exec_failed;
    }

    // load ELF file into memory
    uint32_t entry = load_elf_file(task, name, new_page_dir);    
    if (entry == 0) {
        goto exec_failed;
    }

    // prepare user stack space, reserving space for environment and parameters
    uint32_t stack_top = MEM_TASK_STACK_TOP - MEM_TASK_ARG_SIZE;    // reserve a portion of parameter space
    int err = memory_alloc_for_page_dir(new_page_dir,
                            MEM_TASK_STACK_TOP - MEM_TASK_STACK_SIZE,
                            MEM_TASK_STACK_SIZE, PTE_P | PTE_U | PTE_W);
    if (err < 0) {
        goto exec_failed;
    }

    // copy parameters and write them to the top of the stack
    int argc = strings_count(argv);
    err = copy_args((char *)stack_top, new_page_dir, argc, argv);
    if (err < 0) {
        goto exec_failed;
    }

    // he purpose of 'exec' is to replace the current process, so it only requires changing the current process's execution flow
    // when this process resumes execution, it's as if it's starting anew, 
    // so the user stack should be set to its initial state, and the execution address should be set to the program's entry point
    syscall_frame_t * frame = (syscall_frame_t *)(task->tss.esp0 - sizeof(syscall_frame_t));
    frame->eip = entry;
    frame->eax = frame->ebx = frame->ecx = frame->edx = 0;
    frame->esi = frame->edi = frame->ebp = 0;
    frame->eflags = EFLAGS_DEFAULT| EFLAGS_IF;  

    // The kernel stack doesn't need to be modified; it remains unchanged. Subsequent calls to 'memory_destroy_uvm' won't destroy the mapping of the kernel stack
    // However, the user stack needs to be altered. Additionally, the space for the parameters of the call gate should be pushed onto it.
    frame->esp = stack_top - sizeof(uint32_t)*SYSCALL_PARAM_COUNT;

    // switch to new page table
    task->tss.cr3 = new_page_dir;
    mmu_set_page_dir(new_page_dir); 

    // release the original process's content space
    memory_destroy_uvm(old_page_dir);            

    return  0;

exec_failed:    //resource release
    if (new_page_dir) {
        // switch to the old page table and destroy the new page table
        task->tss.cr3 = old_page_dir;
        mmu_set_page_dir(old_page_dir);
        memory_destroy_uvm(new_page_dir);
    }

    return -1;
}

/**
 * @brief Return Task pid
 */
int sys_getpid (void) {
    task_t * curr_task = task_current();
    return curr_task->pid;
}


/**
 * @brief Waiting for the child process to exit
 */
int sys_wait(int* status) {
    task_t * curr_task = task_current();

    for (;;) {
        // traverse to find processes in a zombie state, then reclaim them. If none are found, enter a sleeping state
        mutex_lock(&task_table_mutex);
        for (int i = 0; i < TASK_NR; i++) {
            task_t * task = task_table + i;
            if (task->parent != curr_task) {
                continue;
            }

            if (task->state == TASK_ZOMBIE) {
                int pid = task->pid;

                *status = task->status;

                memory_destroy_uvm(task->tss.cr3);
                memory_free_page(task->tss.esp0 - MEM_PAGE_SIZE);
                kernel_memset(task, 0, sizeof(task_t));

                mutex_unlock(&task_table_mutex);
                return pid;
            }
        }
        mutex_unlock(&task_table_mutex);

        // not found, wait
        irq_state_t state = irq_enter_protection();
        task_set_block(curr_task);
        curr_task->state = TASK_WAITING;
        task_dispatch();
        irq_leave_protection(state);
    }
}

/**
 * @brief Exit process
 */
void sys_exit(int status) {
    task_t * curr_task = task_current();

    // Close all opened files. The standard input and output libraries will be closed by newlib, but we still handle them here
    for (int fd = 0; fd < TASK_OFILE_NR; fd++) {
        file_t * file = curr_task->file_table[fd];
        if (file) {
            sys_close(fd);
            curr_task->file_table[fd] = (file_t *)0;
        }
    }

    int move_child = 0;

    // find all child processes and hand them over to the init process
    mutex_lock(&task_table_mutex);
    for (int i = 0; i < TASK_OFILE_NR; i++) {
        task_t * task = task_table + i;
        if (task->parent == curr_task) {
            // if there are child processes, transfer them to the 'init_task'
            task->parent = &task_manager.first_task;

            // if there are zombie processes among the child processes, wake up to reclaim resources. 
            // they are not reclaimed by the current process because it is about to exit
            if (task->state == TASK_ZOMBIE) {
                move_child = 1;
            }
        }
    }
    mutex_unlock(&task_table_mutex);

    irq_state_t state = irq_enter_protection();

    // if there are orphaned child processes, wake up the init process
    task_t * parent = curr_task->parent;
    if (move_child && (parent != &task_manager.first_task)) {  // if the parent process is the init process, wake up below
        if (task_manager.first_task.state == TASK_WAITING) {
            task_set_ready(&task_manager.first_task);
        }
    }

    // if a parent task is waiting in a 'wait' state, wake it up for reclamation
    // if the parent process is not waiting, keep handling the zombie state
    if (parent->state == TASK_WAITING) {
        task_set_ready(curr_task->parent);
    }

    // save the return value and enter the zombie state
    curr_task->status = status;
    curr_task->state = TASK_ZOMBIE;
    task_set_block(curr_task);
    task_dispatch();

    irq_leave_protection(state);
}
