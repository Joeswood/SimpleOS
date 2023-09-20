/**
 * Kernel Initialization and Testing Code
 */
#include "comm/boot_info.h"
#include "comm/cpu_instr.h"
#include "cpu/cpu.h"
#include "cpu/irq.h"
#include "dev/time.h"
#include "tools/log.h"
#include "core/task.h"
#include "os_cfg.h"
#include "tools/klib.h"
#include "tools/list.h"
#include "ipc/sem.h"
#include "core/memory.h"
#include "dev/console.h"
#include "dev/kbd.h"
#include "fs/fs.h"

static boot_info_t * init_boot_info;        // boot info

/**
 * kernel entry
 */
void kernel_init (boot_info_t * boot_info) {
    init_boot_info = boot_info;

    // initilize cpu and reload
    cpu_init();
    irq_init();
    log_init();

    // memory init should put in front of file system(tty device)
    memory_init(boot_info);
    fs_init();

    time_init();

    task_manager_init();
}


/**
 *  Init to first task
 */
void move_to_first_task(void) {
    // NOTE: can not use jmp far to enter because of the priority level
    task_t * curr = task_current();
    ASSERT(curr != 0);

    tss_t * tss = &(curr->tss);

    // use inline assembly
    __asm__ __volatile__(
        "push %[ss]\n\t"			// SS
        "push %[esp]\n\t"			// ESP
        "push %[eflags]\n\t"           // EFLAGS
        "push %[cs]\n\t"			// CS
        "push %[eip]\n\t"		    // ip
        "iret\n\t"::[ss]"r"(tss->ss),  [esp]"r"(tss->esp), [eflags]"r"(tss->eflags),
        [cs]"r"(tss->cs), [eip]"r"(tss->eip));
}

void init_main(void) {
    log_printf("==============================");
    log_printf("Kernel is running....");
    log_printf("Version: %s, name: %s", OS_VERSION, "tiny x86 os");
    log_printf("==============================");

    // init task
    task_first_init();
    move_to_first_task();
}
