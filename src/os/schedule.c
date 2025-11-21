#include <stdlib.h>
#include "mcu.h"
#include "schedule.h"
#include "heap.h"
#include "timer.h"

TCB_t *RunPt;
TCB_t *NextRunPt;
TCB_t *IdleThread = {NULL};
TCB_t RunPtBlackHole = {.state=DEAD};

#define PRIORITY_LVL_CNT 5
#define IDLE_PRIORITY PRIORITY_LVL_CNT // alias
static uint32_t LifetimeThreadCount = 0;
static uint16_t ActivePriorityCount[PRIORITY_LVL_CNT+1] = {0}; // count the number of active threads in each priority level
static TCB_t *ThreadSchedule[PRIORITY_LVL_CNT+1]; // tracks pointers to link-list of each priority schedule
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
    TCB_t *idle_tcb = (TCB_t *)malloc(sizeof(TCB_t));
    if (idle_tcb == NULL) return;
    uint8_t *stack = (uint8_t *)malloc(128);
    if (stack == NULL) return;
    idle_tcb->id = 0;
    idle_tcb->priority = IDLE_PRIORITY;
    idle_tcb->state = IDLE;
    idle_tcb->stack = stack;
    // initialize idle thread stack
    //*(uint32_t *)&stack[128-4] = 0x01000000;        // PSR
    *(uint32_t *)&stack[128-8] = (uint32_t)suspend; // PC (function pointer)
    *(uint32_t *)stack = 0xDEADBEEF;                // magic value for stack overflow detection
    idle_tcb->sp = &stack[128-64];                  // set thread SP
    IdleThread = idle_tcb;
    ActivePriorityCount[IDLE_PRIORITY] = 1;

    // launch first thread
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

    // launch first task
    __asm ("LDR     SP, [%0]":: "r" (RunPt)); // Load @ RunPt to get SP
    __asm ("ADD     SP, SP, #56"); // Advance stack pointer to PC
    __asm ("POP     {LR}");        // load initial function pointer into LR from stack
    __asm ("CPSIE   I");           // enable interrupts
    __asm ("BX      LR");          // branch to first thread
}

// add new thread to schedule
uint32_t add_thread(void(*task)(void), uint32_t stack_size, uint32_t priority) {
    uint32_t primask = start_critical();

    if (priority >= PRIORITY_LVL_CNT) return 1;

    TCB_t *newtcb = (TCB_t *)malloc(sizeof(TCB_t));
    if (newtcb == NULL) {
        end_critical(primask);
        return 1;
    }
    uint8_t *stack = (uint8_t *)malloc(stack_size);
    if (stack == NULL) {
        free(newtcb);
        end_critical(primask);
        return 1;
    }

    LifetimeThreadCount++;
    newtcb->id = LifetimeThreadCount;
    newtcb->priority = priority;
    newtcb->stack = stack;

    // initialize thread stack
    *(uint32_t *)&stack[stack_size-4] = 0x01000000;     // PSR
    *(uint32_t *)&stack[stack_size-8] = (uint32_t)task; // PC (function pointer)
    *(uint32_t *)stack = 0xDEADBEEF;                    // magic value for stack overflow detection
    newtcb->sp = &stack[stack_size-64];                 // set thread SP

    // add thread to schedule
    enqueue_thread(newtcb);

    end_critical(primask);
    return 0;
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

// schedule next thread
// simple alias for schedule()
void suspend(void) __attribute__((alias("schedule")));

// perform context switch
__attribute__((naked)) void pendSV_handler(void) {
    // disable interrupts
    __asm ("CPSID  I");
    // save context
    __asm ("PUSH   {R4-R11}"); 
    // get address of run pointer
    __asm ("LDR    R0, =RunPt");
    __asm ("LDR    R1, [R0]");
    //save old stack pointer
    __asm ("MOV    R2, SP");
    __asm ("STR    R2, [R1]");
    // update RunPt with NextRunPt
    __asm ("LDR    R2, =NextRunPt");   // get address of next run pointer
    __asm ("LDR    R1, [R2]");
    __asm ("STR    R1, [R0]");
    // load new SP
    __asm ("LDR    R3, [R1]");
    __asm ("MOV    SP, R3");
    // restore context
    __asm ("POP    {R4-R11}");
    // enable interrupts
    __asm ("CPSIE I");
    // branch to new task
    // LR holds a magic value
    // r0–r3, r12, lr, pc, xpsr are restored automatically in hardware
    __asm ("BX LR");
}

void systick_handler(void) {
    schedule();
}
