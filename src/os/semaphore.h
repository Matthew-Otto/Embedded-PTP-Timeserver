#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "schedule.h"

typedef struct TCB TCB_t;
typedef struct semaphore semaphore_t;
struct semaphore {
    int32_t value;   
    TCB_t *bthreads_root;
};

// initializes semaphore
void init_semaphore(semaphore_t *sem, int32_t value);

// sets semaphore to 1
// blocks until semaphore is acquired
void b_wait(semaphore_t *sem);

// set semaphore to 0
// unblocks first (if any) thread blocked by this semaphore
void b_signal(semaphore_t *sem);

// decrements semaphore
// blocks until semaphore is acquired
void c_wait(semaphore_t *sem);

// increments semaphore
// unblocks first (if any) thread blocked by this semaphore
void c_signal(semaphore_t *sem);

#endif // SEMAPHORE_H