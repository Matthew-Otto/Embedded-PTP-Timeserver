#ifndef SCHEDULE_H
#define SCHEDULE_H

typedef enum {RUNNING, ACTIVE, SLEEPING, BLOCKED, DEAD, IDLE} ThreadState;

typedef struct TCB TCB_t;
struct TCB {
    uint8_t *sp;
    TCB_t *next_tcb;
    TCB_t *prev_tcb;
    uint8_t id;
    ThreadState state;
    uint32_t resume_tick;
    uint8_t priority;
    uint8_t *stack;
};


static inline void disable_interrupts(void){
    __asm ("CPSID I");
}

static inline void enable_interrupts(void){
    __asm ("CPSIE I");
}

// saves current interrupt status before disabling
static inline uint32_t start_critical(void){
    uint32_t primask;
    __asm (
        "MRS %0, PRIMASK\n"
        "CPSID I" 
        : "=r" (primask) :: "memory");
    return primask;
}

// restores previous interrupt status (disabled/enabled)
static inline void end_critical(uint32_t primask){
    __asm ("MSR PRIMASK, %0" :: "r" (primask) : "memory");
}

// go to low power mode while waiting for the next interrupt
static inline void wait_for_interrupt(void){
    __asm ("WFI");
}


void schedule(void);
void init_scheduler(uint32_t timeslice);
uint32_t add_thread(void(*task)(void), uint32_t stack_size, uint32_t priority);

void sleep(uint32_t sleep_time);
void unsleep(void);
void enqueue_thread(TCB_t *thread);
void dequeue_thread(TCB_t *thread);
void suspend(void);

void pendSV_handler(void);
void systick_handler(void);

#endif // SCHEDULE_H
