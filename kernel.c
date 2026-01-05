/* kernel.c - Main kernel with null process and memory tests */
#include "types.h"
#include "serial.h"
#include "string.h"
#include "interrupt.h"
#include "src/memory.h"
#include "src/process.h"
#include "src/scheduler.h"

#define MAX_INPUT 128

extern uint32_t __kernel_end;

static void serial_put_u32(uint32_t value) {
    char buf[11];
    uint32_t i = 0;

    if (value == 0) {
        serial_putc('0');
        return;
    }

    while (value > 0 && i < sizeof(buf)) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (i > 0) {
        serial_putc(buf[--i]);
    }
}

static void serial_put_hex32(uint32_t value) {
    static const char* hex = "0123456789ABCDEF";
    serial_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        serial_putc(hex[(value >> shift) & 0xF]);
    }
}

static uint32_t g_rng_state = 0xC0FFEE01;

static uint32_t parse_u32(const char* s, uint32_t* out_ok) {
    uint32_t value = 0;
    uint32_t ok = 0;

    if (!s || !*s) {
        if (out_ok) *out_ok = 0;
        return 0;
    }

    while (*s == ' ') s++;
    while (*s >= '0' && *s <= '9') {
        ok = 1;
        value = (value * 10) + (uint32_t)(*s - '0');
        s++;
    }

    if (out_ok) *out_ok = ok;
    return value;
}

static int starts_with(const char* s, const char* prefix) {
    if (!s || !prefix) return 0;
    while (*prefix) {
        if (*s++ != *prefix++) return 0;
    }
    return 1;
}

// Process functions
static void proc_p1(void* arg) {
    (void)arg;
    process_t* p = process_current();

    for (uint32_t i = 0; i < 10; i++) {
        serial_puts("[p1] msg=");
        serial_put_u32(i);
        serial_puts(" pid=");
        serial_put_u32(p ? p->pid : 0);
        serial_puts("\n");
        
        // Check periodically if quantum expired and yield
        for (volatile uint32_t j = 0; j < 10000000; j++) {
            if ((j % 10000000) == 0 && scheduler_should_switch()) {
                process_yield();
            }
        }
    }
}

static void proc_p2(void* arg) {
    (void)arg;
    process_t* p = process_current();

    for (uint32_t i = 0; i < 10; i++) {
        serial_puts("[p2] msg=");
        serial_put_u32(i);
        serial_puts(" pid=");
        serial_put_u32(p ? p->pid : 0);
        serial_puts("\n");
        
        // Check periodically if quantum expired and yield
        for (volatile uint32_t j = 0; j < 20000000; j++) {
            if ((j % 10000000) == 0 && scheduler_should_switch()) {
                process_yield();
            }
        }
    }
}

static void proc_p3(void* arg) {
    (void)arg;
    process_t* p = process_current();

    for (uint32_t i = 0; i < 10; i++) {
        serial_puts("[p3] msg=");
        serial_put_u32(i);
        serial_puts(" pid=");
        serial_put_u32(p ? p->pid : 0);
        serial_puts("\n");
        
        // Check periodically if quantum expired and yield
        for (volatile uint32_t j = 0; j < 30000000; j++) {
            if ((j % 10000000) == 0 && scheduler_should_switch()) {
                process_yield();
            }
        }
    }
}

static void proc_p4(void* arg) {
    (void)arg;
    process_t* p = process_current();

    for (uint32_t i = 0; i < 10; i++) {
        serial_puts("[p4] msg=");
        serial_put_u32(i);
        serial_puts(" pid=");
        serial_put_u32(p ? p->pid : 0);
        serial_puts("\n");
        
        // Check periodically if quantum expired and yield
        for (volatile uint32_t j = 0; j < 40000000; j++) {
            if ((j % 10000000) == 0 && scheduler_should_switch()) {
                process_yield();
            }
        }
    }
}

static void proc_p5(void* arg) {
    (void)arg;
    process_t* p = process_current();

    for (uint32_t i = 0; i < 10; i++) {
        serial_puts("[p5] msg=");
        serial_put_u32(i);
        serial_puts(" pid=");
        serial_put_u32(p ? p->pid : 0);
        serial_puts("\n");
        
        // Check periodically if quantum expired and yield
        for (volatile uint32_t j = 0; j < 50000000; j++) {
            if ((j % 10000000) == 0 && scheduler_should_switch()) {
                process_yield();
            }
        }
    }
}

// Commands
static void cmd_ps(void) {
    serial_puts("PID\tSTATE\t\tNAME\n");
    for (uint32_t i = 0; i < process_capacity(); i++) {
        process_t* p = process_at(i);
        if (!p || !p->used || p->state == PROCESS_STATE_UNUSED) {
            continue;
        }
        serial_put_u32(p->pid);
        serial_puts("\t");
        serial_puts(process_state_str(p->state));
        serial_puts("\t");
        if (p->state == PROCESS_STATE_WAITING_IPC) {
            serial_puts("\t");
        }
        serial_puts(p->name);
        serial_puts("\n");
    }
}

static void cmd_kill(uint32_t pid) {
    int32_t rc = process_terminate(pid, 0);
    if (rc < 0) {
        serial_puts("kill: failed\n");
        return;
    }
    serial_puts("killed pid=");
    serial_put_u32(pid);
    serial_puts("\n");
}

static void cmd_run(uint32_t pid) {
    process_t* p = process_get(pid);
    if (!p) {
        serial_puts("run: no such pid\n");
        return;
    }
    if (!p->entry) {
        serial_puts("run: no entry function\n");
        return;
    }

    int32_t rc = process_set_current(pid);
    if (rc < 0) {
        serial_puts("run: cannot set current\n");
        return;
    }

    serial_puts("run: executing pid=");
    serial_put_u32(pid);
    serial_puts("\n");

    p->entry(p->arg);

    process_terminate(pid, 0);
    serial_puts("run: finished pid=");
    serial_put_u32(pid);
    serial_puts("\n");
}

static void cmd_runq(void) {
    // Get the first ready process and start it
    // Timer interrupts will preemptively switch between processes
    uint32_t pid = 0;
    int32_t rc = process_readyq_dequeue(&pid);
    
    if (rc < 0) {
        serial_puts("runq: no ready processes\n");
        return;
    }
    
    // Re-enqueue for round-robin
    process_readyq_enqueue(pid);
    
    serial_puts("runq: starting timer-assisted cooperative scheduling with pid=");
    serial_put_u32(pid);
    serial_puts("\n");
    
    // Enable interrupts to allow timer preemption
    enable_interrupts();
    
    // Start the first process - timer will preempt and switch between them
    int32_t next_pid = scheduler_context_switch();
    
    serial_puts("runq: returned from scheduler, next_pid=");
    serial_put_u32((uint32_t)next_pid);
    serial_puts("\n");
    serial_puts("runq: all processes completed\n");
    (void)next_pid;
}

static void cmd_sched_stats(void) {
    scheduler_stats_t stats = scheduler_get_stats();
    serial_puts("=== Scheduler Statistics ===\n");
    serial_puts("Context switches: ");
    serial_put_u32(stats.total_context_switches);
    serial_puts("\n");
    serial_puts("Quantum expiries: ");
    serial_put_u32(stats.total_quantum_expiries);
    serial_puts("\n");
    serial_puts("Current quantum used: ");
    serial_put_u32(stats.current_quantum_used);
    serial_puts("/");
    serial_put_u32(scheduler_get_quantum());
    serial_puts(" ms\n");
    serial_puts("Ready queue size: ");
    serial_put_u32(scheduler_ready_queue_size());
    serial_puts("\n");
}

void kmain(void) {
    char input[MAX_INPUT];
    int pos = 0;

    /* Initialize hardware */
    serial_init();

    /* Initialize memory manager (heap + stack) */
    memory_init((uint32_t)&__kernel_end);
    serial_puts("Memory manager initialized\n\n");

    /* Initialize process manager */
    process_init();
    serial_puts("Process manager initialized\n\n");

    /* Initialize scheduler */
    scheduler_init(SCHEDULER_DEFAULT_QUANTUM);
    serial_puts("Scheduler initialized (Round-Robin)\n\n");

    /* Initialize interrupts and timer */
    interrupt_init();
    timer_init(100);  // 100 Hz = 10ms per tick
    serial_puts("Interrupts and timer initialized\n\n");

    // Hardcoded ready processes for quick testing
    serial_puts("Creating process p1...\n");
    process_create("p1", proc_p1, NULL, 0);
    serial_puts("Created p1\n");
    
    serial_puts("Creating process p2...\n");
    process_create("p2", proc_p2, NULL, 0);
    serial_puts("Created p2\n");
    
    serial_puts("Creating process p3...\n");
    process_create("p3", proc_p3, NULL, 0);
    serial_puts("Created p3\n");
    
    serial_puts("Creating process p4...\n");
    process_create("p4", proc_p4, NULL, 0);
    serial_puts("Created p4\n");
    
    serial_puts("Creating process p5...\n");
    process_create("p5", proc_p5, NULL, 0);
    serial_puts("Created p5\n\n");

    /* ================= HEAP TESTS ================= */
    serial_puts("=== HEAP TESTS ===\n");

    char* h1 = (char*)kmalloc(16);
    strcpy(h1, "Heap1");
    serial_puts("Allocated h1: "); serial_puts(h1); serial_puts("\n");

    char* h2 = (char*)kmalloc(32);
    strcpy(h2, "Heap2");
    serial_puts("Allocated h2: "); serial_puts(h2); serial_puts("\n");

    kfree(h1); // Free first block
    serial_puts("Freed h1\n");

    char* h3 = (char*)kmalloc(8); // Should reuse h1 space if merged/split
    strcpy(h3, "H3");
    serial_puts("Allocated h3 after free: "); serial_puts(h3); serial_puts("\n");

    kfree(h2);
    kfree(h3);
    serial_puts("Freed h2 and h3\n");

    /* ================= STACK TESTS ================= */
    serial_puts("\n=== STACK TESTS ===\n");

    char* s1 = (char*)kalloc_stack(1024);
    strcpy(s1, "Stack1");
    serial_puts("Allocated s1: "); serial_puts(s1); serial_puts("\n");

    char* s2 = (char*)kalloc_stack(512);
    strcpy(s2, "Stack2");
    serial_puts("Allocated s2: "); serial_puts(s2); serial_puts("\n");

    kfree_stack(s2); // Free last stack first
    serial_puts("Freed s2\n");

    kfree_stack(s1); // Free first stack
    serial_puts("Freed s1\n");

    serial_puts("\nAll memory tests completed successfully!\n\n");

    /* ================= WELCOME MESSAGE ================= */
    serial_puts("========================================\n");
    serial_puts("    kacchiOS - Minimal Baremetal OS\n");
    serial_puts("========================================\n");
    serial_puts("Hello from kacchiOS!\n");
    serial_puts("Running null process...\n\n");
    serial_puts("Commands: ps | run PID | runq | kill PID | sched\n\n");

    /* Main loop - the "null process" */
    while (1) {
        serial_puts("kacchiOS> ");
        pos = 0;

        /* Read input line */
        while (1) {
            char c = serial_getc();

            if (c == '\r' || c == '\n') {
                input[pos] = '\0';
                serial_puts("\n");
                break;
            } else if ((c == '\b' || c == 0x7F) && pos > 0) {
                pos--;
                serial_puts("\b \b");  /* Erase character */
            } else if (c >= 32 && c < 127 && pos < MAX_INPUT - 1) {
                input[pos++] = c;
                serial_putc(c);
            }
        }

        if (pos > 0) {
            if (strcmp(input, "ps") == 0) {
                cmd_ps();
            } else if (starts_with(input, "run ")) {
                uint32_t ok = 0;
                uint32_t pid = parse_u32(input + 4, &ok);
                if (!ok) {
                    serial_puts("usage: run PID\n");
                } else {
                    cmd_run(pid);
                }
            } else if (starts_with(input, "kill ")) {
                uint32_t ok = 0;
                uint32_t pid = parse_u32(input + 5, &ok);
                if (!ok) {
                    serial_puts("usage: kill PID\n");
                } else {
                    cmd_kill(pid);
                }
            } else if (strcmp(input, "runq") == 0) {
                cmd_runq();
            } else if (strcmp(input, "sched") == 0) {
                cmd_sched_stats();
            } else {
                serial_puts("Unknown command. Try: ps | run PID | runq | kill PID | sched\n");
            }
        }
    }

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
