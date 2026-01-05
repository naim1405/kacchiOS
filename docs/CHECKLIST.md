# kacchiOS - Grading Checklist

```
+-------------------+--------------------------------+--------------------------------+--------------------------------+
|    Weights →      |      Must Include              |       Good to have             |          Bonus                 |
|                   |     (7 x 10 = 70%)             |      (4 x 5 = 20%)             |      (4 x 2.5 = 10%)           |
+-------------------+--------------------------------+--------------------------------+--------------------------------+
| Memory            | • Stack allocation        [✓]  | • Stack deallocation      [✓]  | • Optimized memory        [✓]  |
| Manager           | • Heap allocation         [✓]  | • Heap deallocation       [✓]  |   allocation                   |
|                   |                                |                                |   (splitting & coalescing)     |
|                   |                                |                                |                                |
|                   | Score: 20/20 (100%)            | Score: 10/10 (100%)            | Score: 2.5/2.5 (100%)          |
+-------------------+--------------------------------+--------------------------------+--------------------------------+
| Process           | • Process table           [✓]  | • Utility functions to         | • Add more states         [✓]  |
| Manager           | • Process creation        [✓]  |   get process specific         |   (6 states implemented)       |
|                   | • State transition        [✓]  |   functions               [✓]  | • Inter-process           [✓]  |
|                   | • Process termination     [✓]  |   (process_get, process_       |   communication (IPC)          |
|                   |                                |    current, process_at, etc.)  |   (mailbox-based)              |
|                   | Score: 20/20 (100%)            | Score: 5/5 (100%)              | Score: 5/5 (100%)              |
+-------------------+--------------------------------+--------------------------------+--------------------------------+
| Scheduler         | • Clear policy to              | • Configurable time       [✓]  | • Implement Aging         [✗]  |
|                   |   schedule                [✓]  |   quantum                      |                                |
|                   |   (Round-robin)                |   (scheduler_set_quantum)      |                                |
|                   | • Context switch          [✓]  |                                |                                |
|                   |   (ctxsw assembly)             |                                |                                |
|                   |                                |                                |                                |
|                   | Score: 20/20 (100%)            | Score: 5/5 (100%)              | Score: 0/2.5 (0%)              |
+-------------------+--------------------------------+--------------------------------+--------------------------------+

Total Score: 87.5/100 (87.5%)

Breakdown:
- Must Include:   60/60 (100%)
- Good to have:   20/20 (100%)
- Bonus:          7.5/10 (75%)
```

## Implementation Details

### Memory Manager ✅
**Must Include (20/20):**
- ✓ Stack allocation: `kalloc_stack()` for process stacks
- ✓ Heap allocation: `kmalloc()` with first-fit algorithm

**Good to have (10/10):**
- ✓ Stack deallocation: `kfree_stack()` implemented
- ✓ Heap deallocation: `kfree()` with immediate coalescing

**Bonus (2.5/2.5):**
- ✓ Optimized memory allocation: Block splitting on allocation, adjacent block merging on deallocation

---

### Process Manager ✅
**Must Include (20/20):**
- ✓ Process table: Fixed 32-process capacity with PCBs
- ✓ Process creation: `process_create()` with stack allocation and initialization
- ✓ State transition: `process_set_state()` with validation
- ✓ Process termination: `process_terminate()` with stack cleanup

**Good to have (5/5):**
- ✓ Utility functions:
  - `process_get(pid)` - lookup by PID
  - `process_current()` - get current process
  - `process_at(index)` - iterate process table
  - `process_count()` - count active processes
  - `process_capacity()` - get table size
  - `process_state_str()` - state to string conversion
  - `process_restart(pid)` - restart terminated process

**Bonus (5/5):**
- ✓ Additional states (6 total):
  1. UNUSED
  2. READY
  3. RUNNING
  4. BLOCKED
  5. WAITING_IPC
  6. TERMINATED
- ✓ Inter-process communication:
  - Mailbox-based IPC (8 messages per process)
  - `ipc_send()` and `ipc_receive()` functions
  - Automatic wake-up on message arrival

---

### Scheduler ⚠️
**Must Include (20/20):**
- ✓ Clear scheduling policy: Round-robin with timer-assisted cooperative multitasking
- ✓ Context switch: Assembly implementation (`ctxsw` and `start_process`)

**Good to have (5/5):**
- ✓ Configurable time quantum:
  - `scheduler_set_quantum(ms)` to change quantum
  - `scheduler_get_quantum()` to query current quantum
  - Default: 10ms

**Bonus (0/2.5):**
- ✗ Aging: Not implemented (no priority adjustment mechanism)

---

## Summary

kacchiOS successfully implements **87.5%** of the grading criteria, achieving perfect scores in Memory Manager and Process Manager, with only the bonus aging feature missing from the Scheduler.

**Strengths:**
- Complete implementation of all "Must Include" requirements
- Full implementation of all "Good to have" features
- Advanced features: optimized memory allocation, 6-state process model, IPC system

**Missing:**
- Aging mechanism for priority scheduling (bonus feature)
