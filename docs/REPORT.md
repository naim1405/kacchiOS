# kacchiOS: A Minimal Baremetal Operating System with Timer-Assisted Cooperative Multitasking

## Abstract

This report presents the design and implementation of kacchiOS, a minimal baremetal operating system for the x86 architecture. The system implements fundamental OS components including dynamic memory management, process management, and a timer-assisted cooperative scheduler. Built from scratch without relying on existing operating system services, kacchiOS demonstrates core concepts of low-level systems programming, process lifecycle management, and context switching mechanisms inspired by the XINU operating system.

---

## 1. Introduction

### 1.1 Background

Operating systems serve as the intermediary between hardware and application software, managing system resources and providing essential services. Understanding OS internals requires hands-on experience with low-level components such as memory management, process scheduling, and hardware interaction.

### 1.2 Objectives

The primary objectives of this project were to:

1. Implement a functional baremetal operating system for the x86 architecture
2. Design and implement a heap-based dynamic memory allocator
3. Develop a process management subsystem with complete lifecycle control
4. Create a timer-assisted cooperative scheduler with context switching
5. Integrate hardware interrupt handling for timer-based time tracking
6. Provide a command-line interface for system interaction and testing

### 1.3 System Architecture

kacchiOS is structured as a monolithic kernel with the following major components:

- **Boot Module**: Multiboot-compliant bootloader entry point
- **Memory Manager**: Heap allocation with block splitting and coalescing
- **Process Manager**: Process control blocks, state management, and ready queue
- **Scheduler**: Round-robin cooperative scheduling with configurable time quantum
- **Interrupt System**: IDT setup, PIC configuration, and timer interrupt handling
- **I/O Subsystem**: Serial port driver for console I/O
- **Context Switcher**: Assembly routines for low-level CPU state preservation

---

## 2. Memory Manager

### 2.1 Design Overview

The memory manager implements a first-fit heap allocator using an explicit free list. Memory blocks are managed through a linked list structure where each block contains metadata about its size, allocation status, and pointer to the next block.

### 2.2 Data Structures

```c
typedef struct block {
    uint32_t size;          // Size of usable memory in this block
    uint8_t free;           // Allocation status (1 = free, 0 = allocated)
    struct block* next;     // Pointer to next block in list
} block_t;
```

The heap is initialized with a single large free block spanning the available memory region, currently configured for 1MB of heap space.

### 2.3 Allocation Strategy

**kmalloc(size)** implements first-fit allocation:

1. Traverses the free list to find the first block with sufficient size
2. If a block is found, splits it if there's excess space (creating a new free block)
3. Marks the block as allocated and returns a pointer to the usable memory
4. Returns NULL if no suitable block exists (out of memory condition)

The splitting operation ensures minimal internal fragmentation by creating new blocks when allocated space exceeds requested size by more than the block header overhead.

### 2.4 Deallocation and Coalescing

**kfree(ptr)** performs deallocation with immediate coalescing:

1. Marks the block as free
2. Calls merge_blocks() to combine adjacent free blocks
3. Reduces external fragmentation by consolidating free space

The coalescing algorithm makes a single pass through the free list, merging consecutive free blocks to maintain larger contiguous free regions.

### 2.5 Memory Layout

```
Kernel Memory Layout:
+------------------+  0x00100000 (1MB)
|   Kernel Code    |
|   & Data (.text, |
|    .data, .bss)  |
+------------------+  __kernel_end
|                  |
|   Heap (1MB)     |
|   (kmalloc)      |
|                  |
+------------------+
|   Stack Region   |
|  (process stacks)|
+------------------+
```

---

## 3. Process Manager

### 3.1 Process Control Block

Each process is represented by a Process Control Block (PCB) containing:

```c
typedef struct process {
    uint8_t used;                    // Slot allocation status
    uint32_t pid;                    // Process ID
    process_state_t state;           // Current state
    char name[PROCESS_NAME_MAX];     // Process name
    process_entry_t entry;           // Entry function pointer
    void* arg;                       // Argument to entry function
    void* stack_base;                // Stack bottom
    uint32_t stack_size;             // Stack size in bytes
    void* stack_top;                 // Stack top
    void* context;                   // Saved ESP (stack pointer)
    int32_t exit_code;              // Exit status
    // IPC mailbox fields...
} process_t;
```

### 3.2 Process States

The system implements a six-state process model:

- **UNUSED**: PCB slot is free
- **READY**: Process is created and eligible for execution
- **RUNNING**: Process is currently executing
- **BLOCKED**: Process is waiting (reserved for future use)
- **WAITING_IPC**: Process is waiting for inter-process communication
- **TERMINATED**: Process has finished execution

### 3.3 Process Creation

**process_create(name, entry, arg, stack_size)**:

1. Allocates a free PCB slot from the process table (capacity: 32 processes)
2. Allocates stack memory using kmalloc()
3. Initializes stack with context frame for first execution
4. Sets up initial CPU context on the stack:
   ```
   Stack layout (grows downward):
   [process_wrapper address]  <- Return address
   [EBP = 0]
   [EBX = 0]
   [ESI = 0]
   [EDI = 0]
   ```
5. Enqueues process to ready queue
6. Returns PID on success

### 3.4 Stack Initialization

The stack is initialized with volatile pointers to prevent compiler optimizations from disrupting the carefully constructed initial frame. This was critical for preventing triple-faults when using -O2 optimization.

### 3.5 Process Termination and Restart

**process_terminate(pid, exit_code)**:
- Sets state to TERMINATED
- Frees stack memory
- Returns current process to kernel (PID 0)

**process_restart(pid)**:
- Re-allocates stack for terminated process
- Re-initializes stack frame
- Transitions state back to READY
- Re-enqueues to ready queue

This enables processes to be executed multiple times without recreation.

### 3.6 Ready Queue

A circular FIFO queue manages ready processes:
- Implemented as a fixed-size array (capacity: 32 PIDs)
- Head and tail pointers with wraparound
- Automatic enqueueing on process creation
- Dequeue operation skips stale entries (non-READY processes)

---

## 4. Scheduler

### 4.1 Scheduling Algorithm

kacchiOS implements a **timer-assisted cooperative round-robin scheduler**:

- **Time Quantum**: 10ms per process (configurable)
- **Scheduling Policy**: Round-robin with equal time slices
- **Cooperation Model**: Processes periodically check if quantum expired and voluntarily yield

### 4.2 Timer Integration

The scheduler leverages hardware timer interrupts:

1. **PIT (Programmable Interval Timer)** configured at 100Hz (10ms ticks)
2. **PIC (Programmable Interrupt Controller)** remapped to avoid CPU exception conflicts
3. **IDT (Interrupt Descriptor Table)** entry 0x20 points to timer ISR

Timer interrupt handler:
```c
void timer_handler(void) {
    timer_ticks++;
    scheduler_on_tick();    // Decrement quantum
    // Send EOI to PIC
}
```

### 4.3 Cooperative Yielding

Processes integrate yield checks in their work loops:

```c
for (volatile uint32_t j = 0; j < iterations; j++) {
    if ((j % 10000000) == 0 && scheduler_should_switch()) {
        process_yield();
    }
}
```

This hybrid approach combines:
- **Timer tracking** of CPU time usage
- **Process cooperation** for actual context switches

### 4.4 Context Switching

**scheduler_context_switch()** orchestrates process transitions:

1. Dequeues next ready process
2. Re-enqueues for round-robin continuation
3. Updates process states
4. Calls assembly routine for CPU context switch

### 4.5 Assembly Context Switch (ctxsw)

```assembly
ctxsw:
    # Save callee-saved registers
    pushl   %ebp
    pushl   %ebx
    pushl   %esi
    pushl   %edi
    
    # Save old ESP
    movl    20(%esp), %eax      # old_sp
    movl    %esp, (%eax)
    
    # Load new ESP
    movl    24(%esp), %edx      # new_sp
    movl    (%edx), %esp
    
    # Restore registers
    popl    %edi
    popl    %esi
    popl    %ebx
    popl    %ebp
    
    ret  # Return to new process
```

Key aspects:
- Saves only callee-saved registers (caller-saved handled by C calling convention)
- Context is simply the stack pointer (ESP)
- Return address on stack determines where execution resumes

### 4.6 First-Time Process Start

**start_process()** performs one-way jump from kernel to first process:

```assembly
start_process:
    movl    4(%esp), %eax    # Get context pointer
    movl    (%eax), %esp     # Load new ESP
    
    popl    %edi
    popl    %esi
    popl    %ebx
    popl    %ebp
    
    ret  # Jump to process_wrapper
```

---

## 5. Integration and Workflow

### 5.1 System Initialization Sequence

```
boot.S (Assembly)
    ↓
kmain() (C)
    ↓
1. serial_init()           - Serial port I/O
2. memory_init()           - Heap allocator
3. process_init()          - Process table & kernel process
4. scheduler_init()        - Quantum setup
5. interrupt_init()        - IDT configuration
6. timer_init(100)         - PIT @ 100Hz
7. process_create() × 5    - Create p1-p5
    ↓
Main Command Loop (null process)
```

### 5.2 Process Execution Flow

**User Command: `runq`**

```
1. cmd_runq()
    ↓
2. process_readyq_dequeue()   - Get first ready process
    ↓
3. enable_interrupts()        - Allow timer ticks
    ↓
4. scheduler_context_switch()
    ↓
5. start_process()            - Jump to process
    ↓
6. process_wrapper()
    ↓
7. p->entry(p->arg)           - Execute process code
    ↓
8. [Process yields when quantum expires]
    ↓
9. ctxsw()                    - Switch to next process
    ↓
10. [Repeat 7-9 until all processes complete]
    ↓
11. process_wrapper() returns - Back to kernel
    ↓
12. disable_interrupts()
    ↓
13. Return to command prompt
```

### 5.3 Interrupt Handling Flow

```
Timer IRQ0 (every 10ms)
    ↓
CPU vectors to IDT[0x20]
    ↓
isr_timer (Assembly)
    - pushal (save all registers)
    - call timer_handler()
    - popal (restore registers)
    - iret (return from interrupt)
    ↓
timer_handler() (C)
    - Increment tick counter
    - scheduler_on_tick()
      * Decrement quantum
      * Set switch flag if expired
    - Send EOI to PIC
    ↓
Return to interrupted code
    ↓
Process checks scheduler_should_switch()
    ↓
If true, calls process_yield()
```

### 5.4 Command Interface

The kernel provides interactive commands:

- **ps**: List all processes with PID, state, and name
- **ready PID**: Restart terminated process to READY state
- **run PID**: Execute specific process immediately
- **runq**: Run all ready processes with cooperative scheduling
- **kill PID**: Terminate a process

---

## 6. Output

### 6.1 System Boot

[Screenshot placeholder: Clean boot showing welcome message]

### 6.2 Process Listing

```
kacchiOS> ps
PID     STATE           NAME
0       RUNNING         kernel
1       READY           p1
2       READY           p2
3       READY           p3
4       READY           p4
5       READY           p5
```

### 6.3 Cooperative Scheduling Execution

[Screenshot placeholder: runq command showing interleaved process output]

Example output pattern:
```
[p1] msg=0 pid=1
[p2] msg=0 pid=2
[p1] msg=1 pid=1
[p3] msg=0 pid=3
[p2] msg=1 pid=2
...
```

The interleaving demonstrates time-sliced execution with processes yielding cooperatively.

### 6.4 Process Restart

```
kacchiOS> ready 1
ready: restarted pid=1

kacchiOS> ps
PID     STATE           NAME
1       READY           p1
```

---

## 7. Limitations

### 7.1 Cooperative Nature

The scheduler relies on processes voluntarily yielding. A malicious or buggy process that never checks quantum expiration could monopolize the CPU. True preemptive scheduling would require:

1. Context switching directly from interrupt context
2. Manipulating interrupt stack frames
3. More complex synchronization mechanisms

### 7.2 No Priority Scheduling

All processes receive equal time quanta. The system lacks:
- Priority levels
- Dynamic priority adjustment
- Real-time guarantees
- Aging mechanisms

### 7.3 Limited Process Capacity

Static allocation limits:
- Maximum 32 processes (PROCESS_MAX)
- Fixed 1MB heap
- No virtual memory or paging

### 7.4 Synchronization Primitives

The system lacks:
- Mutexes and semaphores
- Condition variables
- Deadlock detection/prevention
- Critical section protection

IPC mailboxes exist but are rudimentary.

### 7.5 Memory Management Constraints

- No memory protection between processes
- First-fit allocation may cause fragmentation
- No garbage collection
- No virtual memory or process isolation

### 7.6 Hardware Limitations

- x86-only (no portability)
- Single-core assumption
- No disk I/O or filesystem
- Serial-only I/O (no keyboard driver)
- No VGA/framebuffer support

### 7.7 Error Handling

Minimal error recovery:
- No exception handling for page faults
- Limited validation of user inputs
- No kernel panic/assertion framework

---

## 8. Conclusions

### 8.1 Achievements

This project successfully demonstrates fundamental operating system concepts through a working implementation:

1. **Functional Memory Management**: A working heap allocator with splitting and coalescing provides dynamic memory for process stacks and kernel data structures.

2. **Complete Process Lifecycle**: Processes can be created, executed, terminated, and restarted, with proper state management throughout their lifecycle.

3. **Timer-Assisted Scheduling**: The hybrid cooperative model effectively shares CPU time among multiple processes while maintaining simplicity.

4. **Hardware Integration**: Successful configuration of PIT, PIC, and IDT demonstrates understanding of x86 interrupt architecture.

5. **Context Switching**: Low-level assembly routines correctly preserve and restore CPU state, enabling process switching.

### 8.2 Educational Value

The project provided hands-on experience with:

- Baremetal programming without OS support
- x86 assembly language for critical operations
- Memory layout and stack management
- Interrupt-driven programming
- Process abstraction implementation
- Debugging at the hardware level (QEMU, GDB, serial output)

### 8.3 Design Tradeoffs

**Cooperative vs. Preemptive**: The cooperative approach simplified implementation by avoiding complex interrupt-context switching but sacrificed true preemption. This tradeoff was acceptable for an educational system.

**Static vs. Dynamic**: Fixed-size arrays for process table and ready queue simplified implementation but limited scalability. Production systems would use dynamic structures.

**Optimization Level**: Disabling GCC optimizations (-O0) was necessary to prevent stack initialization corruption, highlighting the challenges of systems programming where compiler assumptions may break carefully constructed low-level code.

### 8.4 Future Enhancements

Potential extensions to kacchiOS include:

1. **True Preemptive Scheduling**: Implement interrupt-context switching by manipulating saved registers on the stack.

2. **Synchronization Primitives**: Add mutexes, semaphores, and condition variables for safe concurrent programming.

3. **Memory Protection**: Implement paging and virtual memory for process isolation.

4. **File System**: Add a simple filesystem (e.g., FAT) with disk I/O.

5. **System Calls**: Implement a syscall interface for user-space programs.

6. **Multi-core Support**: Extend scheduler for SMP with per-CPU run queues.

7. **Priority Scheduling**: Implement multi-level feedback queue scheduling.

### 8.5 Final Remarks

Building an operating system from scratch provides invaluable insight into how modern systems like Linux and Windows manage hardware resources. kacchiOS, despite its simplicity, demonstrates that the core concepts of process management, scheduling, and memory allocation are implementable with fundamental data structures and careful low-level programming.

The challenges encountered—particularly with compiler optimizations and context switching—reinforced the importance of understanding the full stack from hardware to high-level abstractions. This project serves as a foundation for understanding more complex operating system designs and their engineering tradeoffs.

---

## References

1. Douglas Comer, "Operating System Design: The XINU Approach", CRC Press
2. Intel Corporation, "Intel 64 and IA-32 Architectures Software Developer's Manual"
3. OSDev Wiki, "https://wiki.osdev.org/"
4. Remzi H. Arpaci-Dusseau and Andrea C. Arpaci-Dusseau, "Operating Systems: Three Easy Pieces"

---

## Appendix: Key Source Files

### A.1 File Structure

```
kacchiOS/
├── boot.S              - Bootloader entry point
├── kernel.c            - Main kernel and command interface
├── context.S           - Context switching assembly
├── isr.S               - Interrupt service routines
├── interrupt.c/h       - Interrupt system
├── serial.c/h          - Serial I/O driver
├── string.c/h          - String utilities
├── src/
│   ├── memory.c/h      - Memory manager
│   ├── process.c/h     - Process manager
│   └── scheduler.c/h   - Scheduler
├── link.ld             - Linker script
└── Makefile            - Build system
```

### A.2 Build Instructions

```bash
# Build kernel
make clean && make

# Run in QEMU
make run

# Debug with GDB
make debug
```

### A.3 System Requirements

- GCC with 32-bit support (gcc-multilib)
- GNU Assembler (gas)
- QEMU (qemu-system-i386)
- Linux development environment
