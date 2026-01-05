#include "process.h"

#include "memory.h"

// Forward declare to avoid circular dependency
int32_t scheduler_context_switch(void);

// External assembly context switch routine
extern void ctxsw(void** old_sp, void** new_sp);

// Helper to wrap process entry
static void process_wrapper(void) {
    process_t* p = process_current();
    if (p && p->entry) {
        p->entry(p->arg);
    }
    // Process returned, terminate it
    if (p) {
        process_terminate(p->pid, 0);
    }
    
    // Try to switch to another process
    // If no processes left, halt the system
    while (1) {
        if (process_readyq_count() == 0) {
            // No more processes - halt forever
            while(1) {
                __asm__ volatile("hlt");
            }
        }
        process_yield();
    }
}

#define PID_KERNEL 0

static process_t g_process_table[PROCESS_MAX];
static uint32_t g_next_pid = 1;
static uint32_t g_current_pid = PID_KERNEL;

static uint32_t g_readyq[PROCESS_MAX];
static uint32_t g_readyq_head = 0;
static uint32_t g_readyq_tail = 0;
static uint32_t g_readyq_count = 0;

static void readyq_init(void) {
    g_readyq_head = 0;
    g_readyq_tail = 0;
    g_readyq_count = 0;
}

static void copy_name(char dst[PROCESS_NAME_MAX], const char* src) {
    uint32_t i = 0;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    for (; i + 1 < PROCESS_NAME_MAX && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static int32_t find_free_slot(void) {
    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        if (!g_process_table[i].used) {
            return (int32_t)i;
        }
    }

    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        if (g_process_table[i].used && g_process_table[i].state == PROCESS_STATE_TERMINATED) {
            return (int32_t)i;
        }
    }

    return -1;
}

static void reset_pcb(process_t* p) {
    p->used = 0;
    p->pid = 0;
    p->state = PROCESS_STATE_UNUSED;
    p->name[0] = '\0';
    p->entry = NULL;
    p->arg = NULL;
    p->stack_base = NULL;
    p->stack_size = 0;
    p->stack_top = NULL;
    p->context = NULL;
    p->exit_code = 0;

    p->mailbox_head = 0;
    p->mailbox_tail = 0;
    p->mailbox_count = 0;
}

void process_init(void) {
    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        reset_pcb(&g_process_table[i]);
    }

    readyq_init();

    // Reserve PID 0 as the "kernel/null" process.
    g_process_table[0].used = 1;
    g_process_table[0].pid = PID_KERNEL;
    g_process_table[0].state = PROCESS_STATE_RUNNING;
    copy_name(g_process_table[0].name, "kernel");
    g_process_table[0].entry = NULL;
    g_process_table[0].arg = NULL;
    g_process_table[0].stack_base = NULL;
    g_process_table[0].stack_size = 0;
    g_process_table[0].stack_top = NULL;
    g_process_table[0].context = NULL;
    g_process_table[0].exit_code = 0;

    g_process_table[0].mailbox_head = 0;
    g_process_table[0].mailbox_tail = 0;
    g_process_table[0].mailbox_count = 0;

    g_current_pid = PID_KERNEL;
    g_next_pid = 1;
}

void process_readyq_clear(void) {
    readyq_init();
}

uint32_t process_readyq_count(void) {
    return g_readyq_count;
}

int32_t process_readyq_enqueue(uint32_t pid) {
    process_t* p = process_get(pid);
    if (!p) return -1;
    if (p->state == PROCESS_STATE_TERMINATED || p->state == PROCESS_STATE_UNUSED) return -2;
    if (g_readyq_count >= PROCESS_MAX) return -3;

    g_readyq[g_readyq_tail] = pid;
    g_readyq_tail = (g_readyq_tail + 1) % PROCESS_MAX;
    g_readyq_count++;
    return 0;
}

int32_t process_readyq_dequeue(uint32_t* out_pid) {
    if (!out_pid) return -2;

    // Skip stale entries (terminated/non-ready), keep FIFO order for valid READY ones.
    while (g_readyq_count > 0) {
        uint32_t pid = g_readyq[g_readyq_head];
        g_readyq_head = (g_readyq_head + 1) % PROCESS_MAX;
        g_readyq_count--;

        process_t* p = process_get(pid);
        if (!p) {
            continue;
        }
        if (p->state != PROCESS_STATE_READY) {
            continue;
        }

        *out_pid = pid;
        return 0;
    }

    return -1;
}

process_t* process_get(uint32_t pid) {
    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        if (g_process_table[i].used && g_process_table[i].pid == pid) {
            return &g_process_table[i];
        }
    }
    return NULL;
}

process_t* process_current(void) {
    return process_get(g_current_pid);
}

uint32_t process_capacity(void) {
    return PROCESS_MAX;
}

process_t* process_at(uint32_t index) {
    if (index >= PROCESS_MAX) {
        return NULL;
    }
    return &g_process_table[index];
}

uint32_t process_count(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < PROCESS_MAX; i++) {
        if (g_process_table[i].used && g_process_table[i].state != PROCESS_STATE_UNUSED) {
            count++;
        }
    }
    return count;
}

int32_t process_create(const char* name, process_entry_t entry, void* arg, uint32_t stack_size) {
    int32_t slot = find_free_slot();
    if (slot < 0) {
        return -1; // table full
    }

    if (stack_size == 0) {
        stack_size = PROCESS_DEFAULT_STACK_SIZE;
    }

    void* stack = kmalloc(stack_size);
    if (!stack) {
        return -2; // out of memory
    }

    process_t* p = &g_process_table[(uint32_t)slot];

    // If reusing a terminated slot, free its previous stack (if any).
    if (p->used && p->stack_base) {
        kfree(p->stack_base);
    }

    p->used = 1;
    p->pid = g_next_pid++;
    p->state = PROCESS_STATE_READY;
    copy_name(p->name, name ? name : "proc");

    p->entry = entry;
    p->arg = arg;

    p->stack_base = stack;
    p->stack_size = stack_size;
    p->stack_top = (void*)((uint32_t)stack + stack_size);

    // Initialize stack for context switching
    // Stack grows downward, so start from top
    // Use volatile to prevent optimizer from breaking this
    volatile uint32_t* stack_ptr = (volatile uint32_t*)p->stack_top;
    
    // Push initial context onto stack to match what start_process/ctxsw expects
    // When start_process loads this ESP and does pops + ret, it should:
    // - pop EDI, ESI, EBX, EBP (all 0)
    // - ret (pops return address and jumps there)
    --stack_ptr; *((uint32_t*)stack_ptr) = (uint32_t)process_wrapper;  // Return address
    --stack_ptr; *((uint32_t*)stack_ptr) = 0;  // EBP
    --stack_ptr; *((uint32_t*)stack_ptr) = 0;  // EBX
    --stack_ptr; *((uint32_t*)stack_ptr) = 0;  // ESI  
    --stack_ptr; *((uint32_t*)stack_ptr) = 0;  // EDI
    
    // Context IS the stack pointer (not a pointer to a struct)
    p->context = (void*)stack_ptr;

    p->exit_code = 0;

    p->mailbox_head = 0;
    p->mailbox_tail = 0;
    p->mailbox_count = 0;

    // Newly created process is READY, so enqueue it.
    int32_t qrc = process_readyq_enqueue(p->pid);
    if (qrc < 0) {
        // Roll back allocation if we can't queue it.
        if (p->stack_base) {
            kfree(p->stack_base);
        }
        reset_pcb(p);
        return -3;
    }

    return (int32_t)p->pid;
}

int32_t process_set_state(uint32_t pid, process_state_t new_state) {
    process_t* p = process_get(pid);
    if (!p) {
        return -1;
    }

    if (new_state == PROCESS_STATE_UNUSED) {
        return -2;
    }

    if (p->state == PROCESS_STATE_TERMINATED && new_state != PROCESS_STATE_TERMINATED) {
        return -3;
    }

    p->state = new_state;
    return 0;
}

int32_t process_set_current(uint32_t pid) {
    process_t* next = process_get(pid);
    if (!next) {
        return -1;
    }
    if (next->state == PROCESS_STATE_TERMINATED || next->state == PROCESS_STATE_UNUSED) {
        return -2;
    }

    process_t* curr = process_current();
    if (curr && curr->pid != pid && curr->state == PROCESS_STATE_RUNNING) {
        curr->state = PROCESS_STATE_READY;
    }

    g_current_pid = pid;
    next->state = PROCESS_STATE_RUNNING;
    return 0;
}

int32_t process_terminate(uint32_t pid, int32_t exit_code) {
    if (pid == PID_KERNEL) {
        return -2; // cannot terminate kernel process
    }

    process_t* p = process_get(pid);
    if (!p) {
        return -1;
    }

    p->state = PROCESS_STATE_TERMINATED;
    p->exit_code = exit_code;

    if (p->stack_base) {
        kfree(p->stack_base);
        p->stack_base = NULL;
        p->stack_top = NULL;
        p->stack_size = 0;
    }

    if (g_current_pid == pid) {
        g_current_pid = PID_KERNEL;
        g_process_table[0].state = PROCESS_STATE_RUNNING;
    }

    return 0;
}

const char* process_state_str(process_state_t state) {
    switch (state) {
        case PROCESS_STATE_UNUSED: return "UNUSED";
        case PROCESS_STATE_READY: return "READY";
        case PROCESS_STATE_RUNNING: return "RUNNING";
        case PROCESS_STATE_BLOCKED: return "BLOCKED";
        case PROCESS_STATE_WAITING_IPC: return "WAITING_IPC";
        case PROCESS_STATE_TERMINATED: return "TERMINATED";
        default: return "UNKNOWN";
    }
}

void process_yield(void) {
    // Cooperatively yield CPU to next process
    scheduler_context_switch();
}

static void ipc_copy_payload(uint8_t* dst, const uint8_t* src, uint32_t length) {
    for (uint32_t i = 0; i < length; i++) {
        dst[i] = src[i];
    }
}

int32_t ipc_send(uint32_t to_pid, const void* data, uint32_t length) {
    if (!data && length > 0) {
        return -2;
    }
    if (length > IPC_MAX_PAYLOAD) {
        return -3;
    }

    process_t* to = process_get(to_pid);
    if (!to) {
        return -1;
    }
    if (to->state == PROCESS_STATE_TERMINATED || to->state == PROCESS_STATE_UNUSED) {
        return -4;
    }
    if (to->mailbox_count >= IPC_MAILBOX_CAPACITY) {
        return -5; // mailbox full
    }

    process_t* from = process_current();
    uint32_t from_pid = from ? from->pid : PID_KERNEL;

    ipc_message_t* msg = &to->mailbox[to->mailbox_tail];
    msg->from_pid = from_pid;
    msg->length = length;
    if (length > 0) {
        ipc_copy_payload(msg->payload, (const uint8_t*)data, length);
    }

    to->mailbox_tail = (to->mailbox_tail + 1) % IPC_MAILBOX_CAPACITY;
    to->mailbox_count++;

    // Wake up a process that was waiting on IPC.
    if (to->state == PROCESS_STATE_WAITING_IPC) {
        to->state = PROCESS_STATE_READY;
    }

    return 0;
}

int32_t ipc_receive(uint32_t pid, ipc_message_t* out) {
    if (!out) {
        return -2;
    }

    process_t* p = process_get(pid);
    if (!p) {
        return -1;
    }
    if (p->state == PROCESS_STATE_TERMINATED || p->state == PROCESS_STATE_UNUSED) {
        return -4;
    }

    if (p->mailbox_count == 0) {
        // No scheduler yet; we still record intent to wait.
        if (p->state != PROCESS_STATE_RUNNING) {
            p->state = PROCESS_STATE_WAITING_IPC;
        }
        return -5; // empty
    }

    ipc_message_t* msg = &p->mailbox[p->mailbox_head];
    *out = *msg;

    p->mailbox_head = (p->mailbox_head + 1) % IPC_MAILBOX_CAPACITY;
    p->mailbox_count--;

    return 0;
}
