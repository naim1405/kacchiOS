# Scheduler Implementation Documentation

## Overview
This document describes the round-robin scheduler implementation for kacchiOS.

## What Was Implemented

### 1. **Scheduler Header File** ([src/scheduler.h](src/scheduler.h))
- Defines scheduler API and data structures
- Round-robin scheduling with configurable time quantum (default: 10ms)
- Scheduler statistics tracking
- Function declarations for context switching and tick handling

### 2. **Scheduler Implementation** ([src/scheduler.c](src/scheduler.c))
- **`scheduler_init()`** - Initialize scheduler with time quantum
- **`scheduler_context_switch()`** - Switch to next ready process
- **`scheduler_on_tick()`** - Called on timer ticks to manage time quantum
- **`scheduler_next_process()`** - Get next process to run
- **`scheduler_add_process()`** / **`scheduler_remove_process()`** - Manage ready queue
- Statistics tracking for context switches, quantum expiries

### 3. **Kernel Integration** ([kernel.c](kernel.c))
- Added scheduler initialization in `kmain()`
- New command: `sched` - Display scheduler statistics
- Integrated scheduler with process management system

### 4. **Build System** ([Makefile](Makefile))
- Added `src/scheduler.o` to build dependencies
- Scheduler is now compiled with the kernel

## Features

### Round-Robin Scheduling
- **Time Quantum**: Each process gets 10ms of CPU time (configurable)
- **Fair Scheduling**: All processes get equal CPU time
- **Preemption**: Processes are preempted when quantum expires
- **Ready Queue**: Circular FIFO queue for ready processes

### Scheduler Statistics
Run the `sched` command in kacchiOS to see:
- Total context switches performed
- Number of quantum expiries (preemptions)
- Current quantum usage
- Ready queue size

## How to Use

### Building the OS
```bash
cd "/mnt/d/3.2 semister/CSE-3201-Operating-Systems/os project/kacchiOS"
make clean
make
```

### Running the OS
```bash
make run
```

### Testing the Scheduler
Once the OS boots, try these commands:
```
kacchiOS> ps              # List all processes
kacchiOS> spawn 3         # Create 3 dummy processes
kacchiOS> sched           # View scheduler statistics
kacchiOS> runq            # Execute all ready processes
kacchiOS> sched           # Check updated statistics
```

## Git Branch: scheduler

All scheduler implementation is committed to the `scheduler` branch.

### To Push to GitHub
```bash
cd '/mnt/d/3.2 semister/CSE-3201-Operating-Systems/os project/kacchiOS'
git push -u origin scheduler
```

**Note**: You'll need to authenticate with GitHub. Use one of these methods:
1. **Personal Access Token (PAT)** - Recommended
   - Go to GitHub Settings → Developer settings → Personal access tokens
   - Generate a token with `repo` scope
   - Use the token as your password when prompted

2. **SSH Key** - Alternative
   ```bash
   git remote set-url origin git@github.com:naim1405/kacchiOS.git
   git push -u origin scheduler
   ```

3. **GitHub CLI** - Alternative
   ```bash
   gh auth login
   git push -u origin scheduler
   ```

## Commit Message
```
feat: implement round-robin scheduler

- Add src/scheduler.h with scheduler API and data structures
- Add src/scheduler.c with round-robin scheduling implementation
- Update kernel.c to initialize and integrate scheduler
- Update Makefile to include scheduler.o in build
- Add 'sched' command to display scheduler statistics
- Implements timer-tick driven time quantum management
- Provides context switching based on time slices
```

## Architecture

### Scheduler State
```c
- g_quantum_ms                    // Time quantum in milliseconds
- g_current_quantum_remaining     // Remaining quantum for current process
- g_need_switch                   // Flag to trigger context switch
- g_stats                         // Scheduler statistics
```

### Key Functions

1. **Initialization**
   - `scheduler_init(quantum_ms)` - Set up scheduler with time quantum

2. **Scheduling**
   - `scheduler_next_process()` - Peek at next process without switching
   - `scheduler_context_switch()` - Perform actual context switch

3. **Time Management**
   - `scheduler_on_tick()` - Called every timer tick
   - Decrements quantum counter
   - Sets switch flag when quantum expires

4. **Process Management**
   - `scheduler_add_process(pid)` - Add process to ready queue
   - `scheduler_remove_process(pid)` - Remove from scheduling

5. **Statistics & Configuration**
   - `scheduler_get_stats()` - Get scheduling statistics
   - `scheduler_get_quantum()` / `scheduler_set_quantum()` - Manage time quantum
   - `scheduler_ready_queue_size()` - Get number of ready processes

## Future Enhancements

To extend this scheduler, you could:
1. **Priority Scheduling** - Add process priorities
2. **Multiple Queues** - Implement multi-level feedback queue
3. **Real-time Support** - Add deadline scheduling
4. **CPU Affinity** - Support for multi-core systems
5. **Load Balancing** - Distribute processes across CPUs
6. **Advanced Statistics** - Track waiting time, turnaround time, etc.

## Testing

### Manual Testing
```
# Boot the OS
make run

# Test basic scheduling
kacchiOS> spawn 5        # Create 5 processes
kacchiOS> ps             # Verify they're created
kacchiOS> sched          # Check initial stats
kacchiOS> runq           # Run all processes
kacchiOS> sched          # Check final stats
```

### Expected Behavior
- Processes should be scheduled in round-robin order
- Each process gets equal time quantum
- Context switches should increment with each switch
- Quantum expiries should track preemptions

## References
- Process Management: [docs/process.md](docs/process.md)
- Memory Management: [docs/memory.md](docs/memory.md)
