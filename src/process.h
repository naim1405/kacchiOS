#ifndef PROCESS_H
#define PROCESS_H

#include "types.h"

#define PROCESS_MAX 32
#define PROCESS_NAME_MAX 16
#define PROCESS_DEFAULT_STACK_SIZE 4096

#define IPC_MAX_PAYLOAD 64
#define IPC_MAILBOX_CAPACITY 8

typedef enum {
    PROCESS_STATE_UNUSED = 0,
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_WAITING_IPC,
    PROCESS_STATE_TERMINATED
} process_state_t;

typedef void (*process_entry_t)(void* arg);

// CPU context for context switching (i386 registers)
typedef struct {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebx;
    uint32_t ebp;
    uint32_t eip;  // Return address (where to resume)
} cpu_context_t;

typedef struct ipc_message {
    uint32_t from_pid;
    uint32_t length;
    uint8_t payload[IPC_MAX_PAYLOAD];
} ipc_message_t;

typedef struct process {
    uint8_t used;
    uint32_t pid;
    process_state_t state;

    char name[PROCESS_NAME_MAX];

    process_entry_t entry;
    void* arg;

    void* stack_base;
    uint32_t stack_size;
    void* stack_top;

    // CPU context for context switching - this is the stack pointer
    void* context;  // Saved stack pointer (ESP)

    int32_t exit_code;

    // Simple mailbox IPC (fixed-size ring buffer)
    ipc_message_t mailbox[IPC_MAILBOX_CAPACITY];
    uint32_t mailbox_head;
    uint32_t mailbox_tail;
    uint32_t mailbox_count;
} process_t;

void process_init(void);

// Returns PID on success, negative error code on failure.
int32_t process_create(const char* name, process_entry_t entry, void* arg, uint32_t stack_size);

// Returns 0 on success, negative error code on failure.
int32_t process_set_state(uint32_t pid, process_state_t new_state);
int32_t process_set_current(uint32_t pid);
int32_t process_terminate(uint32_t pid, int32_t exit_code);

process_t* process_get(uint32_t pid);
process_t* process_current(void);

// Process table enumeration helpers
uint32_t process_capacity(void);
process_t* process_at(uint32_t index);

uint32_t process_count(void);
const char* process_state_str(process_state_t state);

// Cooperative yielding for multitasking
void process_yield(void);

// Ready queue (FIFO of PIDs)
// Enqueue is typically done automatically by process_create().
int32_t process_readyq_enqueue(uint32_t pid);
int32_t process_readyq_dequeue(uint32_t* out_pid);
uint32_t process_readyq_count(void);
void process_readyq_clear(void);

// IPC
// Returns 0 on success, negative error code on failure.
int32_t ipc_send(uint32_t to_pid, const void* data, uint32_t length);

// Attempts to receive into *out. If the mailbox is empty, returns a negative
// error and moves the target process into PROCESS_STATE_WAITING_IPC.
int32_t ipc_receive(uint32_t pid, ipc_message_t* out);

static inline int32_t ipc_receive_current(ipc_message_t* out) {
    process_t* p = process_current();
    if (!p) return -1;
    return ipc_receive(p->pid, out);
}

#endif
