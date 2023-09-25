/**
 * OS config
 */
#ifndef OS_OS_CFG_H
#define OS_OS_CFG_H

#define GDT_TABLE_SIZE      	256		// GDT number
#define KERNEL_SELECTOR_CS		(1 * 8)		// kernel code descriptor
#define KERNEL_SELECTOR_DS		(2 * 8)		// kernel data descriptor
#define KERNEL_STACK_SIZE       (8*1024)    // kernel stack
#define SELECTOR_SYSCALL     	(3 * 8)	// call gate selector

#define OS_TICK_MS              10       	// number of clock cycles per millisecond

#define OS_VERSION              "0.0.1"     // OS version

#define IDLE_STACK_SIZE       1024        // idle task stack

#define TASK_NR             128            // process number

#define ROOT_DEV            DEV_DISK, 0xb1  // device root dir located in

#endif //OS_OS_CFG_H
