#include "scheduler.h"
#include "process.h"

// External assembly context switch routines
extern void ctxsw(void** old_sp, void** new_sp);
extern void start_process(void** sp);  // One-way jump, never returns

/**
 * Scheduler implementation - Round-robin process scheduling
 */

// Scheduler state
static uint32_t g_quantum_ms = SCHEDULER_DEFAULT_QUANTUM;
static uint32_t g_current_quantum_remaining = 0;
static uint8_t g_need_switch = 0;

// Statistics
static scheduler_stats_t g_stats = {0, 0, 0};

void scheduler_init(uint32_t quantum_ms) {
    if (quantum_ms == 0) {
        quantum_ms = SCHEDULER_DEFAULT_QUANTUM;
    }
    
    g_quantum_ms = quantum_ms;
    g_current_quantum_remaining = g_quantum_ms;
    g_need_switch = 0;
    
    // Reset stats
    g_stats.total_context_switches = 0;
    g_stats.total_quantum_expiries = 0;
    g_stats.current_quantum_used = 0;
}

int32_t scheduler_next_process(void) {
    uint32_t next_pid;
    int32_t rc = process_readyq_dequeue(&next_pid);
    
    if (rc < 0) {
        // No ready process; return kernel process (PID 0)
        return 0;
    }
    
    // Re-enqueue for next time (round-robin)
    process_readyq_enqueue(next_pid);
    
    return (int32_t)next_pid;
}

int32_t scheduler_context_switch(void) {
    process_t* current = process_current();
    
    // Get next ready process
    uint32_t next_pid;
    int32_t rc = process_readyq_dequeue(&next_pid);
    
    if (rc < 0) {
        // No ready process
        return -1;
    }
    
    process_t* next = process_get(next_pid);
    if (!next) {
        return -1;
    }
    
    // Re-enqueue for round-robin
    process_readyq_enqueue(next_pid);
    
    // If switching to the same process, no context switch needed
    if (current && current->pid == next_pid) {
        return (int32_t)next_pid;
    }
    
    // Check if we're starting from kernel (PID 0 or NULL)
    int from_kernel = (!current || current->pid == 0);
    
    // Update process states before switching
    if (current && current->state == PROCESS_STATE_RUNNING && current->pid != 0) {
        current->state = PROCESS_STATE_READY;
    }
    
    next->state = PROCESS_STATE_RUNNING;
    process_set_current(next_pid);
    
    // Reset quantum
    g_current_quantum_remaining = g_quantum_ms;
    g_stats.current_quantum_used = 0;
    g_stats.total_context_switches++;
    g_need_switch = 0;
    
    // Perform the actual context switch
    if (from_kernel) {
        // First time from kernel - just jump to process (no return to kernel)
        // DEBUG: This should never return
        start_process(&next->context);
        // If we get here, something went wrong
        return (int32_t)next_pid;
    } else {
        // Normal process-to-process switch
        ctxsw(&current->context, &next->context);
        // Returns here when this process is scheduled again
        return (int32_t)next_pid;
    }
}

void scheduler_on_tick(void) {
    // Decrement quantum
    if (g_current_quantum_remaining > 0) {
        g_current_quantum_remaining--;
        g_stats.current_quantum_used++;
    }
    
    // If quantum expired, request context switch
    if (g_current_quantum_remaining == 0) {
        g_need_switch = 1;
        g_stats.total_quantum_expiries++;
    }
}

int32_t scheduler_add_process(uint32_t pid) {
    process_t* p = process_get(pid);
    if (!p) {
        return -1;
    }
    
    // Only add if process is READY
    if (p->state != PROCESS_STATE_READY) {
        return -2;
    }
    
    return process_readyq_enqueue(pid);
}

int32_t scheduler_remove_process(uint32_t pid) {
    process_t* p = process_get(pid);
    if (!p) {
        return -1;
    }
    
    // Don't need to remove from queue explicitly;
    // The scheduler skips non-READY processes
    return 0;
}

scheduler_stats_t scheduler_get_stats(void) {
    return g_stats;
}

void scheduler_reset_stats(void) {
    g_stats.total_context_switches = 0;
    g_stats.total_quantum_expiries = 0;
    g_stats.current_quantum_used = 0;
}

uint32_t scheduler_get_quantum(void) {
    return g_quantum_ms;
}

void scheduler_set_quantum(uint32_t quantum_ms) {
    if (quantum_ms > 0) {
        g_quantum_ms = quantum_ms;
    }
}

uint32_t scheduler_ready_queue_size(void) {
    return process_readyq_count();
}

uint8_t scheduler_should_switch(void) {
    return g_need_switch;
}
