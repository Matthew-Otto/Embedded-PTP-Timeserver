#include <stdio.h>
#include <stdint.h>
#include "semaphore.h"
#include "schedule.h"

void init_semaphore(semaphore_t *sem, int32_t value) {
    sem->value = value;
    sem->bthreads_root = NULL;
}

// blocks until semaphore is acquired
void b_wait(semaphore_t *sem) {
    uint32_t primask = start_critical();
    while (sem->value) {
        end_critical(primask);
        sched_block(sem);
        primask = start_critical();
    }
    sem->value = 1;
    __DMB();
    end_critical(primask);
}

// unblocks first (if any) thread blocked by this semaphore
// if unblocked thread is higher priority, schedule it immediately
void b_signal(semaphore_t *sem) {
    uint32_t stat = start_critical();
    sem->value = 0;
    __DMB();
    if (sem->bthreads_root != NULL) {
        if (sched_unblock(sem)) NVIC_SetPendingIRQ(SysTick_IRQn);
    }
    end_critical(stat);
}

// decrements semaphore
// blocks until semaphore is acquired
void c_wait(semaphore_t *sem) {
    uint32_t stat = start_critical();

    while (sem->value == 0) {
        enable_interrupts();
        sched_block(sem);
        disable_interrupts();
    }
    sem->value--;
    __DMB();

    end_critical(stat);
}

// increments semaphore
// unblocks first (if any) thread blocked by this semaphore
void c_signal(semaphore_t *sem) {
    uint32_t stat = start_critical();

    sem->value++;
    __DMB();
    if (sem->bthreads_root != NULL) {
        if (sched_unblock(sem)) NVIC_SetPendingIRQ(SysTick_IRQn);
    }

    end_critical(stat);
}
