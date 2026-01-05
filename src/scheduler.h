#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"
#include "process.h"

#define SCHEDULER_DEFAULT_QUANTUM 10

typedef struct {
    uint32_t total_context_switches;
    uint32_t total_quantum_expiries;
    uint32_t current_quantum_used;
} scheduler_stats_t;

void scheduler_init(uint32_t quantum_ms);

int32_t scheduler_next_process(void);

int32_t scheduler_context_switch(void);

void scheduler_on_tick(void);

int32_t scheduler_add_process(uint32_t pid);

int32_t scheduler_remove_process(uint32_t pid);

scheduler_stats_t scheduler_get_stats(void);

void scheduler_reset_stats(void);

uint32_t scheduler_get_quantum(void);

void scheduler_set_quantum(uint32_t quantum_ms);

uint32_t scheduler_ready_queue_size(void);

uint8_t scheduler_should_switch(void);

#endif
