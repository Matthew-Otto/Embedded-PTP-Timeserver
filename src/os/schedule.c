#include <stdlib.h>
#include <stdbool.h>
#include "mcu.h"
#include "watchdog.h"
#include "schedule.h"
#include "semaphore.h"
#include "heap.h"
#include "timer.h"

#include "gpio.h" // BOZO

TCB_t *RunPt;
TCB_t *NextRunPt;
TCB_t *IdleThread = {NULL};
TCB_t RunPtBlackHole = {.state=DEAD};

#define PRIORITY_LVL_CNT 5
#define IDLE_PRIORITY PRIORITY_LVL_CNT // alias
static uint32_t LifetimeThreadCount = 0;
static uint16_t ActivePriorityCount[PRIORITY_LVL_CNT+1] = {0}; // count the number of active threads in each priority level
static TCB_t *ThreadSchedule[PRIORITY_LVL_CNT]; // tracks pointers to link-list of each priority schedule
static TCB_t *SleepScheduleRoot = NULL;


// schedule next task
void schedule(void) {
    uint32_t primask = start_critical();

    // update current thread / put it back into schedule
    if (RunPt->state == RUNNING) {
        RunPt->state = ACTIVE;
        ActivePriorityCount[RunPt->priority]++;
    }

    // Schedule next thread
    // find highest priority level with at least one active (but not running) thread
    // (idle priority always have active count of 1)
    uint8_t pri = 0;
    while (ActivePriorityCount[pri] == 0) { 
        pri++;
    }
    if (pri == IDLE_PRIORITY) {
        NextRunPt = IdleThread;
    } else {
        NextRunPt = ThreadSchedule[pri];
        ThreadSchedule[pri] = ThreadSchedule[pri]->next_tcb; // point root to next element to be scheduled
        NextRunPt->state = RUNNING;
        ActivePriorityCount[pri]--;
    }

    // trigger pendsv interrupt to context switch
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;

    end_critical(primask);
}

void init_scheduler(uint32_t timeslice /* timeslice in ms */) {
    // Initialize systick timer
    uint32_t clk_speed = get_clock_speed();
    // Set schedule timeslice (in ms)
    SysTick->LOAD = (clk_speed / 1000) * timeslice; // Set reload register (must be < 2^24)
    // Clear current value
    SysTick->VAL = 0;
    // Processor clock as source, enable interrupt, enable counter
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

    // initialize idle task
    add_thread(suspend, 32, IDLE_PRIORITY);

    // schedule first thread
    uint8_t pri = 0;
    while (ActivePriorityCount[pri] == 0) { 
        pri++;
    }
    if (pri == IDLE_PRIORITY) {
        RunPt = IdleThread;
    } else {
        RunPt = ThreadSchedule[pri];
        ThreadSchedule[pri] = ThreadSchedule[pri]->next_tcb; // point root to next element to be scheduled
        RunPt->state = RUNNING;
        ActivePriorityCount[pri]--;
    }

    // configure systick and pendSV interrupt priorities
    NVIC_SetPriority(PendSV_IRQn, 15);
    NVIC_SetPriority(SysTick_IRQn, 14);

    // Execute SVC 0 to launch first task
    __enable_irq();
    __asm("SVC #0");
}

// add new thread to schedule
int add_thread(void(*task)(void), uint32_t stack_size, uint32_t priority) {
    uint32_t primask = start_critical();

    static bool idle_init = 0;

    if (priority > IDLE_PRIORITY) return -1;
    if (priority == IDLE_PRIORITY) {
        if (idle_init) return -1;
        idle_init = 1;
    }

    TCB_t *newtcb = (TCB_t *)malloc(sizeof(TCB_t));
    if (newtcb == NULL) {
        end_critical(primask);
        return -1;
    }
    uint32_t *stack = (uint32_t *)malloc(stack_size << 2);
    if (stack == NULL) {
        free(newtcb);
        end_critical(primask);
        return -1;
    }

    if (priority == IDLE_PRIORITY) {
        newtcb->id = 0;
        newtcb->priority = priority;
    } else {
        LifetimeThreadCount++;
        newtcb->id = LifetimeThreadCount;
        newtcb->priority = priority;
    }
    newtcb->stack = stack;
    newtcb->stack_bottom = stack;
    newtcb->stack_top = stack + stack_size;

    *newtcb->stack_bottom = 0xDEADBEEF;  // magic value for stack overflow detection
    uint32_t *sp = newtcb->stack_top; // stack pointer starts at top of stack

    // initialize thread stack
    *(--sp) = xPSR_T_Msk;     // Set Thumb bit
    *(--sp) = (uint32_t)task; // PC (function pointer)
    *(--sp) = 0xFFFFFFBC;            // LR
    *(--sp) = 0xCCCCCCCC; // R12
    *(--sp) = 0x33333333; // R3
    *(--sp) = 0x22222222; // R2
    *(--sp) = 0x11111111; // R1
    *(--sp) = 0x00000000; // R0
    *(--sp) = 0xBBBBBBBB; // R11
    *(--sp) = 0xAAAAAAAA; // R10
    *(--sp) = 0x99999999; // R9
    *(--sp) = 0x88888888; // R8
    *(--sp) = 0x77777777; // R7
    *(--sp) = 0x66666666; // R6
    *(--sp) = 0x55555555; // R5
    *(--sp) = 0x44444444; // R4
    newtcb->sp = sp;      // set thread SP

    // add thread to schedule
    if (priority == IDLE_PRIORITY) {
        newtcb->state = IDLE;
        IdleThread = newtcb;
        ActivePriorityCount[IDLE_PRIORITY] = 1;
    } else {
        enqueue_thread(newtcb);
    }

    end_critical(primask);
    return 0;
}

// remove thread from schedule
void sched_block(semaphore_t *sem) {
    volatile uint32_t primask = start_critical();

    // BOZO
    toggle_GPIO(GPIOC, GPIO_PIN_10);

    TCB_t *thread = RunPt;
  
    // remove RunPt from thread pool
    dequeue_thread(thread);
    // set thread state to blocked
    thread->state = BLOCKED;

    // add thread to semaphore
    if (sem->bthreads_root == NULL) {
        sem->bthreads_root = thread;
    } else {
        // insert tcb into blocked list based on priority
        TCB_t *blocked_node = sem->bthreads_root;
        if (thread->priority < blocked_node->priority) {
            // insert into front of list
            thread->next_tcb = blocked_node;
            sem->bthreads_root = thread;
        } else {
            while (1) {
                if (blocked_node->next_tcb == NULL) {
                    blocked_node->next_tcb = thread;
                    break;
                } else if (thread->priority < blocked_node->next_tcb->priority) {
                    thread->next_tcb = blocked_node->next_tcb;
                    blocked_node->next_tcb = thread;
                    break;
                }
                blocked_node = blocked_node->next_tcb;
            }
        }
    }

    schedule();
    end_critical(primask);
}

// insert thread back into schedule
bool sched_unblock(semaphore_t *sem) {
    uint32_t primask = start_critical();

    // BOZO
    toggle_GPIO(GPIOC, GPIO_PIN_11);

    TCB_t *thread = sem->bthreads_root;

    // update semaphore blocked list
    sem->bthreads_root = sem->bthreads_root->next_tcb;

    // insert unblocked thread into beginning of active list
    enqueue_thread(thread);

    end_critical(primask);

    // determine if this unblocked thread was higher priority 
    // than the currently running thread
    return (thread->priority >= RunPt->priority);
}

// sleeps thread for <sleep_time> ms
void sleep(uint32_t sleep_time) {
    if (sleep_time == 0) return;
    uint32_t primask = start_critical();

    TCB_t *thread = RunPt;
    
    // calculate when this thread should be re-queued
    thread->resume_tick = get_raw_time_ms() + sleep_time;

    // remove RunPt from thread pool
    dequeue_thread(thread);
    // update thread state
    thread->state = SLEEPING;

    // insert thread into sleep queue and arm timer if necessary
    if (SleepScheduleRoot == NULL) {
        SleepScheduleRoot = thread;
        arm_timer(SLEEP, thread->resume_tick);
    } else {
        // if current thread will resume before the current head of the list
        if (thread->resume_tick < SleepScheduleRoot->resume_tick){
            // insert before SleepScheduleRoot
            thread->next_tcb = SleepScheduleRoot;
            SleepScheduleRoot = thread;
            arm_timer(SLEEP, thread->resume_tick);
        } else {
            // find where the new sleeping thread belongs
            TCB_t *node = SleepScheduleRoot;
            while (1) {
                if (node->next_tcb == NULL) {
                    node->next_tcb = thread;
                    break;
                } else if (thread->resume_tick < node->next_tcb->resume_tick) {
                    thread->next_tcb = node->next_tcb;
                    node->next_tcb = thread;
                    break;
                }
                node = node->next_tcb;
            }
        }
    }

    end_critical(primask);

    schedule();
}

// unsleeps the first thread in the sleep queue
void unsleep(void) {
    if (SleepScheduleRoot == NULL) return; // this shouldn't happen, but just in case
    uint32_t primask = start_critical();

    TCB_t *resumed_thread = SleepScheduleRoot;
    SleepScheduleRoot = SleepScheduleRoot->next_tcb;

    // insert unslept thread into the front of the queue
    enqueue_thread(resumed_thread);

    // arm timer for next thread
    if (SleepScheduleRoot != NULL) {
        arm_timer(SLEEP, SleepScheduleRoot->resume_tick);
    }

    end_critical(primask);
}

void enqueue_thread(TCB_t *thread) {
    uint8_t priority = thread->priority;
    // insert thread into beginning of active list
    if (ThreadSchedule[priority] == NULL) {
        ThreadSchedule[priority] = thread;
        thread->next_tcb = thread;
        thread->prev_tcb = thread;
    } else {
        thread->next_tcb = ThreadSchedule[priority];
        thread->prev_tcb = ThreadSchedule[priority]->prev_tcb;
        thread->prev_tcb->next_tcb = thread;
        ThreadSchedule[priority]->prev_tcb = thread;
        ThreadSchedule[priority] = thread;
    }
    // update thread state
    thread->state = ACTIVE;
    // increment active count for this priority level
    ActivePriorityCount[priority]++;
}

void dequeue_thread(TCB_t *thread) {
    uint8_t priority = thread->priority;
   
    if (ActivePriorityCount[priority] == 0) {
        ThreadSchedule[priority] = NULL;
    } else {
        thread->prev_tcb->next_tcb = thread->next_tcb;
        thread->next_tcb->prev_tcb = thread->prev_tcb;
        ThreadSchedule[priority] = thread->next_tcb;
    }
    thread->next_tcb = NULL;
}

void suspend(void) {
    while (1) __WFI();
}

// perform context switch
__attribute__((naked)) void pendSV_handler(void) {
    // disable interrupts
    __asm ("CPSID  I");
    // save context
    __asm ("MRS    R0, PSP");
    __asm ("STMDB  R0!, {R4-R11}");
    // get address of run pointer
    __asm ("LDR    R3, =RunPt");
    __asm ("LDR    R1, [R3]");
    // save old stack pointer
    __asm ("STR    R0, [R1]");
    // update RunPt with NextRunPt
    __asm ("LDR    R2, =NextRunPt");   // get address of next run pointer
    __asm ("LDR    R1, [R2]");
    __asm ("STR    R1, [R3]");
    // load new stack pointer and restore context
    __asm ("LDR    R0, [R1]");
    __asm ("LDMIA  R0!, {R4-R11}");
    // load new SP
    __asm ("MSR    PSP, R0");
    // enable interrupts
    __asm ("CPSIE   I");
    // branch to new task
    // LR holds a magic value (0xffffffbc)
    // r0–r3, r12, lr, pc, xpsr are restored automatically in hardware
    __asm ("BX LR");
}

void systick_handler(void) {
    kick_watchdog();
    schedule();
}

// launch first task
__attribute__((naked)) void SVC_handler(void)
{
    __asm ("LDR    R0, =RunPt");     // load address of *RunPt
    __asm ("LDR    R1, [R0]");       // load *RunPt
    __asm ("LDR    R2, [R1]");       // dereference *RunPt (get stack pointer
    __asm ("ADD    R2, R2, #32");    // advance stack pointer to location of R0
    __asm ("MSR    PSP, R2");        // set PSP to task stack pointer
    __asm ("ISB"); // flush pipeline
    // TODO reclaim MSP stack space
    __asm ("MOV    LR, 0xFFFFFFBC"); // Unstack from PSP
    __asm ("BX     LR");             // return from 'exception'
}
