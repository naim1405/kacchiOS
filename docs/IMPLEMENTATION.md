# kacchiOS Implementation Guide

This document provides a high-level overview of kacchiOS and its core components.

---

## Project Overview

kacchiOS is a baremetal x86 operating system built from scratch for educational purposes. It runs directly on hardware (or QEMU) without any underlying OS. The system implements three main modules: memory management, process management, and scheduling.

The OS boots from a multiboot-compliant bootloader, initializes hardware, and provides a simple command-line interface where users can create, run, and manage processes.

---

## Implemented Modules

kacchiOS consists of three core modules:

1. **Memory Manager** - Handles dynamic memory allocation
2. **Process Manager** - Manages process lifecycle and state
3. **Scheduler** - Coordinates process execution with timer-assisted cooperative multitasking

---

## Module Descriptions

### 1. Memory Manager

The memory manager provides dynamic memory allocation for the kernel using a heap of 1MB. It uses a first-fit allocation strategy with a linked list of memory blocks.

**Key Functions:**
- `memory_init()` - Initializes the heap
- `kmalloc()` - Allocates memory, splits blocks if needed
- `kfree()` - Frees memory, coalesces adjacent free blocks
- `kalloc_stack()` / `kfree_stack()` - Allocates/frees process stacks

**Features:**
- Block splitting to avoid wasting memory
- Block coalescing to reduce fragmentation
- Separate stack allocation for processes

### 2. Process Manager

The process manager handles the complete lifecycle of processes from creation to termination. It maintains a process table with up to 32 processes and uses a Process Control Block (PCB) to store all process information.

**Process States:**
- UNUSED - Slot is available
- READY - In ready queue, waiting for CPU
- RUNNING - Currently executing
- BLOCKED - Waiting for resources
- WAITING_IPC - Waiting for messages
- TERMINATED - Finished execution

**Key Functions:**
- `process_init()` - Initializes process table
- `process_create()` - Creates a new process with stack and context
- `process_terminate()` - Terminates and frees process resources
- `process_restart()` - Re-initializes terminated processes
- `process_send()` / `process_receive()` - IPC mailbox system

**Features:**
- Fixed set of 5 user processes (p1-p5)
- Ready queue for scheduling
- IPC mailbox for inter-process communication
- Process restart capability

### 3. Scheduler

The scheduler implements timer-assisted cooperative multitasking with round-robin scheduling. A hardware timer tracks time quanta, but processes voluntarily yield when their quantum expires.

**How it Works:**
- Timer interrupt fires every 10ms (100Hz)
- Quantum counter decrements on each tick
- When quantum expires, a flag is set
- Processes periodically check the flag and voluntarily yield
- Context switch saves current process state and loads next process

**Key Functions:**
- `scheduler_init()` - Sets up scheduling parameters
- `scheduler_on_tick()` - Decrements quantum on timer interrupt
- `scheduler_should_switch()` - Checks if process should yield
- `scheduler_context_switch()` - Switches between processes

**Features:**
- Round-robin scheduling (fair time distribution)
- Configurable time quantum (default 10ms)
- Assembly context switching routines
- Quantum tracking and statistics

---

## System Integration

The three modules work together to enable multitasking:

1. Memory manager allocates stacks and data structures for processes
2. Process manager creates processes and maintains the ready queue
3. Scheduler coordinates execution by switching between ready processes
4. Timer interrupts track time usage
5. Processes cooperatively yield when their time is up
6. Control returns to kernel when all processes complete

---

## Commands

The kernel provides a command-line interface with:
- `ps` - List all processes and their states
- `ready <pid>` - Restart a terminated process
- `run <pid>` - Run a specific process
- `runq` - Run all ready processes with round-robin scheduling
- `kill <pid>` - Terminate a running process

---

## Limitations

While kacchiOS successfully implements core OS concepts, it has several limitations:

**Scheduling:**
- Uses cooperative multitasking instead of true preemptive scheduling
- Processes must voluntarily yield; misbehaving processes can monopolize CPU
- No priority-based scheduling or aging mechanism
- Fixed time quantum cannot be adjusted at runtime

**Memory Management:**
- First-fit allocation can cause external fragmentation over time
- No virtual memory or memory protection between processes
- Fixed heap size (1MB) cannot be expanded
- No guard pages to detect stack overflow

**Process Management:**
- Fixed maximum of 32 processes
- No parent-child process relationships
- No dynamic process creation (fixed set of 5 processes)
- Limited IPC (only mailbox-based, no shared memory or pipes)

**Hardware Support:**
- Only supports x86 (i386) architecture
- No disk I/O or file system
- Limited to serial port for I/O
- No network support

**System Features:**
- No user mode vs kernel mode separation
- No system calls interface
- Single-core only (no SMP support)
- Minimal error handling and validation

---

## Conclusion

kacchiOS successfully demonstrates fundamental operating system concepts including dynamic memory allocation, process management, and scheduling. The implementation achieves all core requirements:

- **Memory Manager**: Provides efficient allocation with splitting and coalescing to minimize fragmentation
- **Process Manager**: Implements complete process lifecycle with state tracking and IPC capabilities
- **Scheduler**: Delivers fair CPU time distribution through round-robin scheduling with timer assistance

The project serves its educational purpose by providing hands-on experience with low-level system programming, interrupt handling, context switching, and the integration of multiple OS subsystems. While limited in scope compared to production operating systems, kacchiOS provides a solid foundation for understanding how modern operating systems work at the hardware level.

The cooperative multitasking approach, while not suitable for production systems, simplifies the implementation and makes the code easier to understand for learning purposes. Future enhancements could include implementing true preemptive scheduling, adding priority levels with aging, or introducing virtual memory management.
