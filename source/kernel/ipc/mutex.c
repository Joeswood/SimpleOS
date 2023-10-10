/**
 * Mutex
 */
#include "cpu/irq.h"
#include "ipc/mutex.h"

/**
 * Mutex initization
 */
void mutex_init (mutex_t * mutex) {
    mutex->locked_count = 0;
    mutex->owner = (task_t *)0;
    list_init(&mutex->wait_list);
}

/**
 * Accquire Mutex
 */
void mutex_lock (mutex_t * mutex) {
    irq_state_t  irq_state = irq_enter_protection();

    task_t * curr = task_current();
    if (mutex->locked_count == 0) {
        // no task
        mutex->locked_count = 1;
        mutex->owner = curr;
    } else if (mutex->owner == curr) {
        // owned by current task, add count
        mutex->locked_count++;
    } else {
        // owned by other task, add waiting queue
        task_t * curr = task_current();
        task_set_block(curr);
        list_insert_last(&mutex->wait_list, &curr->wait_node);
        task_dispatch();
    }

    irq_leave_protection(irq_state);
}

/**
 * Release Mutex
 */
void mutex_unlock (mutex_t * mutex) {
    irq_state_t  irq_state = irq_enter_protection();

    // the ownner of the mutex can release
    task_t * curr = task_current();
    if (mutex->owner == curr) {
        if (--mutex->locked_count == 0) {
            // decrease to 0, release the mutex
            mutex->owner = (task_t *)0;

            // if there is task waiting in the queue, wake up and accquire the mutex
            if (list_count(&mutex->wait_list)) {
                list_node_t * task_node = list_remove_first(&mutex->wait_list);
                task_t * task = list_node_parent(task_node, task_t, wait_node);
                task_set_ready(task);

                mutex->locked_count = 1;
                mutex->owner = task;

                task_dispatch();
            }
        }
    }

    irq_leave_protection(irq_state);
}

