/**
 * Task 
 */
#ifndef TASK_H
#define TASK_H

#include "comm/types.h"
#include "cpu/cpu.h"
#include "tools/list.h"
#include "fs/file.h"

#define TASK_NAME_SIZE				32			// length of task name
#define TASK_TIME_SLICE_DEFAULT		10			// timestamp counts
#define TASK_OFILE_NR				128			// Max supported file number

#define TASK_FLAG_SYSTEM       	(1 << 0)		// system task

typedef struct _task_args_t {
	uint32_t ret_addr;		// return addr
	uint32_t argc;
	char **argv;
}task_args_t;

/**
 * @brief Task control structure
 */
typedef struct _task_t {
    enum {
		TASK_CREATED,
		TASK_RUNNING,
		TASK_SLEEP,
		TASK_READY,
		TASK_WAITING,
		TASK_ZOMBIE,
	}state;

    char name[TASK_NAME_SIZE];		// task name

    int pid;				// pid
    struct _task_t * parent;		// parent process
	uint32_t heap_start;		// start addr of heap
	uint32_t heap_end;			// end addr of heap
    int status;				// result of process

    int sleep_ticks;		// sleep time
    int time_slice;			
	int slice_ticks;		// decreasing time slice counter

    file_t * file_table[TASK_OFILE_NR];	// Max number of file a task can open

	tss_t tss;				// TSS segement of task
	uint16_t tss_sel;		// TSS selector
	
	list_node_t run_node;		
	list_node_t wait_node;		
	list_node_t all_node;		
}task_t;

int task_init (task_t *task, const char * name, int flag, uint32_t entry, uint32_t esp);
void task_switch_from_to (task_t * from, task_t * to);
void task_set_ready(task_t *task);
void task_set_block (task_t *task);
void task_set_sleep(task_t *task, uint32_t ticks);
void task_set_wakeup (task_t *task);
int sys_yield (void);
void task_dispatch (void);
task_t * task_current (void);
void task_time_tick (void);
void sys_msleep (uint32_t ms);
file_t * task_file (int fd);
int task_alloc_fd (file_t * file);
void task_remove_fd (int fd);

typedef struct _task_manager_t {
    task_t * curr_task;       

	list_t ready_list;			
	list_t task_list;			// created task list
	list_t sleep_list;        

	task_t first_task;			
	task_t idle_task;			
	int app_code_sel;			// selector of task code
	int app_data_sel;			// elector of task data
}task_manager_t;

void task_manager_init (void);
void task_first_init (void);
task_t * task_first_task (void);

int sys_getpid (void);
int sys_fork (void);
int sys_execve(char *name, char **argv, char **env);
void sys_exit(int status);
int sys_wait(int* status);

#endif

