/**
 * Semaphore
 */
#include "cpu/irq.h"
#include "core/task.h"
#include "ipc/sem.h"

/**
 * Semaphore initization
 */
void sem_init (sem_t * sem, int init_count) {
    sem->count = init_count;
    list_init(&sem->wait_list);
}

/**
 * Acquire Semaphore
 */
void sem_wait (sem_t * sem) {
    irq_state_t  irq_state = irq_enter_protection();

    if (sem->count > 0) {
        sem->count--;
    } else {
        // remove from the ready queue, then add waiting queue
        task_t * curr = task_current();
        task_set_block(curr);
        list_insert_last(&sem->wait_list, &curr->wait_node);
        task_dispatch();
    }

    irq_leave_protection(irq_state);
}

/**
 * Release Semaphore
 */
void sem_notify (sem_t * sem) {
    irq_state_t  irq_state = irq_enter_protection();

    if (list_count(&sem->wait_list)) {
        // if there is process waiting, wake it up and add it to the ready queue
        list_node_t * node = list_remove_first(&sem->wait_list);
        task_t * task = list_node_parent(node, task_t, wait_node);
        task_set_ready(task);

        task_dispatch();
    } else {
        sem->count++;
    }

    irq_leave_protection(irq_state);
}

/**
 * Get current value of Semaphore
 */
int sem_count (sem_t * sem) {
    irq_state_t  irq_state = irq_enter_protection();
    int count = sem->count;
    irq_leave_protection(irq_state);
    return count;
}

