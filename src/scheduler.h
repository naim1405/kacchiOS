#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"
#include "process.h"

/**
 * Scheduler module - Implements round-robin process scheduling
 * 
 * The scheduler manages process scheduling using a time-slice based
 * round-robin (RR) algorithm. Each process gets an equal amount of CPU
 * time (time quantum) before being preempted and moved to the back of
 * the ready queue.
 */

#define SCHEDULER_DEFAULT_QUANTUM 10  // milliseconds per time slice

/**
 * Scheduler statistics structure
 */
typedef struct {
    uint32_t total_context_switches;
    uint32_t total_quantum_expiries;
    uint32_t current_quantum_used;
} scheduler_stats_t;

/**
 * Initialize the scheduler
 * Must be called after process_init()
 */
void scheduler_init(uint32_t quantum_ms);

/**
 * Get the next process to run (without actually switching to it)
 * Returns PID of next process, or -1 if no ready process
 */
int32_t scheduler_next_process(void);

/**
 * Perform a context switch to the next ready process
 * Returns the PID of the new current process, or -1 on error
 * 
 * This is called:
 * - On timer tick when quantum expires
 * - When current process blocks or terminates
 * - From kernel's main loop
 */
int32_t scheduler_context_switch(void);

/**
 * Called on each timer tick (e.g., from interrupt handler)
 * Decrements the current quantum and triggers context switch if expired
 */
void scheduler_on_tick(void);

/**
 * Add a process to the scheduler (called when process becomes READY)
 * Returns 0 on success, negative on error
 */
int32_t scheduler_add_process(uint32_t pid);

/**
 * Remove a process from scheduler (called when process terminates/blocks)
 * Returns 0 on success, negative on error
 */
int32_t scheduler_remove_process(uint32_t pid);

/**
 * Get current scheduler statistics
 */
scheduler_stats_t scheduler_get_stats(void);

/**
 * Reset scheduler statistics
 */
void scheduler_reset_stats(void);

/**
 * Get the current time quantum in milliseconds
 */
uint32_t scheduler_get_quantum(void);

/**
 * Set the time quantum in milliseconds
 */
void scheduler_set_quantum(uint32_t quantum_ms);

/**
 * Get number of processes in ready queue
 */
uint32_t scheduler_ready_queue_size(void);

/**
 * Check if scheduler needs to perform a context switch
 * (quantum expired or current process terminated)
 */
uint8_t scheduler_should_switch(void);

#endif // SCHEDULER_H
