# kacchiOS Implementation Guide

This document explains how each major component of kacchiOS works, from high-level flow to implementation details.

---

## 1. Memory Manager

### General Flow

The memory manager provides dynamic memory allocation for the kernel. When the system boots:

1. `memory_init()` is called with the address right after the kernel code (`__kernel_end`)
2. A single large free block spanning the entire heap (1MB) is created
3. When memory is requested via `kmalloc()`, the allocator searches for a suitable free block
4. If found, the block is split if it's larger than needed, then marked as allocated
5. When memory is freed via `kfree()`, the block is marked free and adjacent free blocks are merged

**Key Concept**: The heap is a linked list of memory blocks, each with metadata about size and allocation status.

### Detailed Implementation

#### Data Structure

```c
typedef struct block {
    uint32_t size;       // Size of usable memory (excludes header)
    uint8_t free;        // 1 = free, 0 = allocated
    struct block* next;  // Pointer to next block
} block_t;
```

Each block has:
- **Header**: `block_t` metadata (12 bytes)
- **Payload**: Actual usable memory returned to caller

Memory layout:
```
+--------+----------------+--------+----------------+
| Header |    Payload     | Header |    Payload     | ...
| (12B)  |   (size bytes) | (12B)  |   (size bytes) |
+--------+----------------+--------+----------------+
  ^                          ^
  |                          |
heap_head                 next pointer
```

#### Initialization

```c
void memory_init(uint32_t heap_start) {
    heap_head = (block_t*)heap_start;
    heap_head->size = HEAP_MAX_SIZE - sizeof(block_t);  // 1MB - 12 bytes
    heap_head->free = 1;
    heap_head->next = NULL;
}
```

Creates one massive free block spanning the entire heap.

#### Allocation (kmalloc)

**Algorithm**: First-fit

```
1. Start at heap_head
2. For each block:
   a. If block is free AND size >= requested_size:
      - Split block if it's much larger than needed
      - Mark block as allocated (free = 0)
      - Return pointer to payload (block_address + sizeof(block_t))
   b. Move to next block
3. If no suitable block found, return NULL (out of memory)
```

**Block Splitting**:
```
Before split (block size = 1000, request = 100):
+--------+------------------+
| Header |    1000 bytes    |
+--------+------------------+

After split:
+--------+----------+--------+----------+
| Header | 100 bytes| Header | 888 bytes|
| free=0 |          | free=1 |          |
+--------+----------+--------+----------+
         ^                   ^
    return this          new free block
```

Splitting only happens if remaining space > `sizeof(block_t)` to avoid tiny unusable blocks.

#### Deallocation (kfree)

```
1. Get block header from pointer (ptr - sizeof(block_t))
2. Mark block as free (free = 1)
3. Call merge_blocks() to coalesce adjacent free blocks
```

**Coalescing Example**:
```
Before merge:
+--------+------+--------+------+--------+------+
| Header | Free | Header | Free | Header | Used |
+--------+------+--------+------+--------+------+

After merge:
+--------+--------------+--------+------+
| Header |   Free       | Header | Used |
+--------+--------------+--------+------+
```

The `merge_blocks()` function makes a single pass through the list, combining consecutive free blocks to reduce fragmentation.

#### Stack Allocation

Separate from heap, stacks grow downward from a high address:

```c
static uint32_t stack_top = 0x200000;  // 2MB

void* kalloc_stack(uint32_t size) {
    stack_top -= size;
    return (void*)stack_top;
}
```

Each call returns a lower address, creating isolated stacks for processes.

---

## 2. Process Manager

### General Flow

The process manager handles the complete lifecycle of processes:

1. **Boot**: `process_init()` sets up the process table and creates PID 0 (kernel process)
2. **Creation**: `process_create()` allocates a PCB, allocates stack, initializes context
3. **Scheduling**: Ready processes are enqueued in a FIFO ready queue
4. **Execution**: Context switch loads CPU state from process stack
5. **Termination**: `process_terminate()` frees stack and marks PCB as terminated
6. **Restart**: `process_restart()` re-allocates stack for terminated processes

**Key Concept**: Each process has a PCB containing all state information and a private stack initialized with a context frame for switching.

### Detailed Implementation

#### Process Control Block (PCB)

```c
typedef struct process {
    // Identity
    uint8_t used;                    // Is this slot occupied?
    uint32_t pid;                    // Process ID
    char name[16];                   // Process name
    
    // State
    process_state_t state;           // UNUSED, READY, RUNNING, etc.
    int32_t exit_code;              // Exit status
    
    // Execution
    process_entry_t entry;           // Entry function pointer
    void* arg;                       // Argument to pass
    
    // Memory
    void* stack_base;                // Bottom of stack
    uint32_t stack_size;             // Stack size in bytes
    void* stack_top;                 // Top of stack
    void* context;                   // Saved ESP (stack pointer)
    
    // IPC
    ipc_message_t mailbox[8];        // Message queue
    uint32_t mailbox_head;           // Read position
    uint32_t mailbox_tail;           // Write position
    uint32_t mailbox_count;          // Message count
} process_t;
```

**Process Table**: Fixed array of 32 PCBs

```c
static process_t g_process_table[PROCESS_MAX];
```

#### Process States

State machine:
```
   UNUSED → READY → RUNNING → TERMINATED
                ↓       ↑
                ↓       ↑
              BLOCKED   ↑
                        ↑
            WAITING_IPC ┘
```

- **UNUSED**: Slot is free, can be allocated
- **READY**: Process created, in ready queue, waiting for CPU
- **RUNNING**: Process is currently executing
- **BLOCKED**: Process waiting (reserved for future use)
- **WAITING_IPC**: Process blocked waiting for message
- **TERMINATED**: Process finished, stack freed

#### Process Creation

**High-level steps**:
```
1. Find free PCB slot
2. Allocate stack (default 4KB)
3. Initialize PCB fields
4. Set up initial stack frame
5. Enqueue to ready queue
6. Return PID
```

**Critical: Stack Initialization**

The stack must be set up so context switching works correctly:

```c
volatile uint32_t* stack_ptr = (volatile uint32_t*)p->stack_top;

// Push initial values (stack grows down)
--stack_ptr; *stack_ptr = (uint32_t)process_wrapper;  // Return address
--stack_ptr; *stack_ptr = 0;  // EBP
--stack_ptr; *stack_ptr = 0;  // EBX
--stack_ptr; *stack_ptr = 0;  // ESI
--stack_ptr; *stack_ptr = 0;  // EDI

p->context = (void*)stack_ptr;  // Save ESP
```

**Stack layout after initialization**:
```
High addresses (stack top)
    |
    v
+-------------------+
|  process_wrapper  |  ← Return address (where ret will jump)
+-------------------+
|      EBP = 0      |
+-------------------+
|      EBX = 0      |
+-------------------+
|      ESI = 0      |
+-------------------+
|      EDI = 0      |  ← ESP points here (p->context)
+-------------------+
|                   |
|   Unused stack    |
|                   |
+-------------------+
Low addresses (stack base)
```

When `start_process()` is called, it:
1. Loads ESP to point to EDI
2. Pops EDI, ESI, EBX, EBP (all zeros)
3. Executes `ret` which pops the return address and jumps to `process_wrapper`

#### Process Wrapper

Every process starts in this wrapper:

```c
static void process_wrapper(void) {
    process_t* p = process_current();
    
    // Call the actual process function
    if (p && p->entry) {
        p->entry(p->arg);
    }
    
    // Process returned - terminate it
    if (p) {
        process_terminate(p->pid, 0);
    }
    
    // Try switching to other processes
    if (process_readyq_count() == 0) {
        // No more processes - return to kernel
        g_current_pid = PID_KERNEL;
        g_process_table[0].state = PROCESS_STATE_RUNNING;
        return;
    }
    
    // Yield to next process
    process_yield();
    
    // Keep yielding while processes exist
    while (process_readyq_count() > 0) {
        process_yield();
    }
    
    // All done - return to kernel
    g_current_pid = PID_KERNEL;
    g_process_table[0].state = PROCESS_STATE_RUNNING;
}
```

This wrapper:
- Calls the process entry function
- Automatically terminates the process when it returns
- Yields to other processes until all are done
- Returns control to kernel when ready queue is empty

#### Ready Queue

FIFO circular buffer:

```c
static uint32_t g_readyq[PROCESS_MAX];
static uint32_t g_readyq_head = 0;
static uint32_t g_readyq_tail = 0;
static uint32_t g_readyq_count = 0;
```

**Enqueue**:
```c
g_readyq[g_readyq_tail] = pid;
g_readyq_tail = (g_readyq_tail + 1) % PROCESS_MAX;
g_readyq_count++;
```

**Dequeue** (with stale entry filtering):
```c
while (g_readyq_count > 0) {
    uint32_t pid = g_readyq[g_readyq_head];
    g_readyq_head = (g_readyq_head + 1) % PROCESS_MAX;
    g_readyq_count--;
    
    process_t* p = process_get(pid);
    if (p && p->state == PROCESS_STATE_READY) {
        *out_pid = pid;
        return 0;  // Found valid process
    }
    // Skip terminated/non-ready processes
}
return -1;  // Queue empty
```

This skips processes that are no longer READY (e.g., terminated while in queue).

#### Process Termination

```c
int32_t process_terminate(uint32_t pid, int32_t exit_code) {
    process_t* p = process_get(pid);
    
    p->state = PROCESS_STATE_TERMINATED;
    p->exit_code = exit_code;
    
    // Free the stack
    if (p->stack_base) {
        kfree(p->stack_base);
        p->stack_base = NULL;
        p->stack_top = NULL;
        p->stack_size = 0;
    }
    
    // If this was current process, return to kernel
    if (g_current_pid == pid) {
        g_current_pid = PID_KERNEL;
        g_process_table[0].state = PROCESS_STATE_RUNNING;
    }
    
    return 0;
}
```

#### Process Restart

Allows running terminated processes again:

```c
int32_t process_restart(uint32_t pid) {
    process_t* p = process_get(pid);
    
    // Allocate new stack
    void* stack = kmalloc(p->stack_size);
    
    // Re-initialize stack frame (same as create)
    volatile uint32_t* stack_ptr = ...;
    --stack_ptr; *stack_ptr = (uint32_t)process_wrapper;
    ...
    p->context = (void*)stack_ptr;
    
    // Reset state
    p->state = PROCESS_STATE_READY;
    p->exit_code = 0;
    
    // Re-enqueue
    return process_readyq_enqueue(p->pid);
}
```

---

## 3. Scheduler

### General Flow

The scheduler coordinates process execution using timer-assisted cooperative multitasking:

1. **Initialization**: Set time quantum (default 10ms)
2. **Timer Tick**: Every 10ms, timer interrupt fires
   - `timer_handler()` calls `scheduler_on_tick()`
   - Quantum counter is decremented
   - Switch flag is set when quantum expires
3. **Cooperative Yield**: Process checks `scheduler_should_switch()`
   - If quantum expired, calls `process_yield()`
   - Triggers context switch to next process
4. **Context Switch**: 
   - Dequeue next ready process
   - Re-enqueue current for round-robin
   - Call assembly routine to switch CPU state
5. **Repeat**: Next process runs until its quantum expires

**Key Concept**: Timer tracks time usage but doesn't force switches - processes voluntarily yield when they detect quantum expiry.

### Detailed Implementation

#### Scheduler State

```c
static uint32_t g_quantum_ms = 10;              // Time slice per process
static uint32_t g_current_quantum_remaining = 10;  // Countdown
static uint8_t g_need_switch = 0;               // Switch flag
static scheduler_stats_t g_stats;               // Statistics
```

#### Timer Integration

**Hardware Setup**:

1. **PIT (Programmable Interval Timer)**: Generates interrupts at 100Hz (every 10ms)

```c
void timer_init(uint32_t frequency_hz) {
    uint32_t divisor = PIT_FREQUENCY / frequency_hz;  // 1193182 / 100
    
    // Configure channel 0, rate generator mode
    outb(0x43, 0x36);
    
    // Send divisor (lobyte then hibyte)
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}
```

2. **PIC (Programmable Interrupt Controller)**: Routes IRQ0 to INT 0x20

```c
// Remap PIC to avoid conflicts with CPU exceptions
outb(0x20, 0x11);  // ICW1: Initialize
outb(0x21, 0x20);  // ICW2: Master base = 0x20
outb(0x21, 0x04);  // ICW3: Slave on IRQ2
outb(0x21, 0x01);  // ICW4: 8086 mode

// Unmask only IRQ0 (timer)
outb(0x21, 0xFE);
```

3. **IDT (Interrupt Descriptor Table)**: Entry 0x20 points to `isr_timer`

```c
idt_set_gate(0x20, (uint32_t)isr_timer, 0x08, 0x8E);
```

**Interrupt Handler Flow**:

```
Hardware Timer Fires (every 10ms)
    ↓
CPU jumps to IDT[0x20] = isr_timer
    ↓
isr_timer (Assembly):
    pushal              // Save all registers
    call timer_handler  // Call C function
    popal               // Restore registers
    iret                // Return to interrupted code
    ↓
timer_handler (C):
    timer_ticks++
    scheduler_on_tick()  // Decrement quantum
    Send EOI to PIC
    ↓
Return to process (no forced switch)
```

#### Quantum Tracking

```c
void scheduler_on_tick(void) {
    // Decrement remaining quantum
    if (g_current_quantum_remaining > 0) {
        g_current_quantum_remaining--;
        g_stats.current_quantum_used++;
    }
    
    // Set flag when quantum expires
    if (g_current_quantum_remaining == 0) {
        g_need_switch = 1;
        g_stats.total_quantum_expiries++;
    }
}
```

**Important**: This only sets a flag - it doesn't force a context switch because we're in interrupt context.

#### Cooperative Yielding

Processes periodically check if they should yield:

```c
for (volatile uint32_t j = 0; j < 50000000; j++) {
    if ((j % 10000000) == 0 && scheduler_should_switch()) {
        process_yield();  // Voluntary yield
    }
}
```

```c
uint8_t scheduler_should_switch(void) {
    return g_need_switch;
}
```

Every 10 million iterations (roughly), the process checks the flag and yields if set.

#### Context Switch

**High-level flow**:

```c
int32_t scheduler_context_switch(void) {
    process_t* current = process_current();
    
    // Get next process from ready queue
    uint32_t next_pid;
    process_readyq_dequeue(&next_pid);
    process_t* next = process_get(next_pid);
    
    // Re-enqueue for round-robin
    process_readyq_enqueue(next_pid);
    
    // Update states
    if (current && current->state == PROCESS_STATE_RUNNING) {
        current->state = PROCESS_STATE_READY;
    }
    next->state = PROCESS_STATE_RUNNING;
    process_set_current(next_pid);
    
    // Reset quantum
    g_current_quantum_remaining = g_quantum_ms;
    g_stats.current_quantum_used = 0;
    g_stats.total_context_switches++;
    g_need_switch = 0;
    
    // Perform actual CPU state switch
    if (from_kernel) {
        start_process(&next->context);  // One-way jump
    } else {
        ctxsw(&current->context, &next->context);  // Bidirectional
    }
}
```

#### Assembly Context Switch (ctxsw)

Saves and restores CPU state:

```assembly
ctxsw:
    # Save current process state
    pushl   %ebp
    pushl   %ebx
    pushl   %esi
    pushl   %edi
    
    # Get arguments from C call
    movl    20(%esp), %eax      # old_sp (pointer to context)
    movl    24(%esp), %edx      # new_sp
    
    # Save current ESP to *old_sp
    movl    %esp, (%eax)
    
    # Load new ESP from *new_sp
    movl    (%edx), %esp
    
    # Restore new process state
    popl    %edi
    popl    %esi
    popl    %ebx
    popl    %ebp
    
    ret  # Return to new process
```

**What happens during context switch**:

```
Process A running:
    Stack A (ESP_A):
    +-------+
    |  EIP  | ← Return to process_yield() in A
    +-------+
    |  EBP  |
    |  EBX  |
    |  ESI  |
    |  EDI  | ← ESP_A points here
    +-------+

After ctxsw(&A->context, &B->context):
    A->context = ESP_A  (saved)
    ESP = ESP_B         (loaded)
    
    Stack B (ESP_B):
    +-------+
    |  EIP  | ← Return to process_yield() in B
    +-------+
    |  EBP  |
    |  EBX  |
    |  ESI  |
    |  EDI  | ← ESP now points here
    +-------+
    
Process B now running!
```

The `ret` instruction pops EIP from Stack B, causing execution to resume in Process B at the point where it last yielded.

#### First-Time Process Start

When switching from kernel to a process for the first time:

```assembly
start_process:
    movl    4(%esp), %eax    # Get context pointer
    movl    (%eax), %esp     # Load process ESP
    
    # Pop initial context (all zeros)
    popl    %edi
    popl    %esi
    popl    %ebx
    popl    %ebp
    
    # Return pops process_wrapper address
    ret  # Jump to process_wrapper
```

This is a one-way jump - kernel doesn't expect to return.

#### Round-Robin Scheduling

The re-enqueueing ensures fair scheduling:

```
Ready queue before dequeue: [2, 3, 4, 5]
                                ^
                             dequeue
                             
Process 2 runs, then:

Ready queue after re-enqueue: [3, 4, 5, 2]
                                           ^
                                       re-enqueue
```

Every process gets exactly one quantum before returning to the back of the queue.

---

## Integration Example: Running All Processes

When user types `runq`:

```
1. cmd_runq() called
2. Dequeue first process (PID 1)
3. Re-enqueue it immediately for round-robin
4. enable_interrupts() - allow timer
5. scheduler_context_switch()
   → start_process() jumps to PID 1
6. Process 1 starts executing in process_wrapper()
   → Calls proc_p1(arg)
7. Timer fires every 10ms, quantum counts down
8. Process 1 checks scheduler_should_switch()
   → Sees quantum expired
   → Calls process_yield()
9. process_yield() → scheduler_context_switch()
   → ctxsw() switches to Process 2
10. Process 2 runs, eventually yields
11. ... continues round-robin through all processes
12. All processes finish and terminate
13. process_wrapper() sees empty ready queue
    → Returns control to kernel
14. disable_interrupts()
15. Return to command prompt
```

---

## Summary

- **Memory Manager**: Dynamic allocation with first-fit and coalescing
- **Process Manager**: PCB-based process tracking with stack-based context
- **Scheduler**: Timer-assisted cooperative round-robin with assembly context switching

These three components work together to provide multitasking capabilities in kacchiOS.
