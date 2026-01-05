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

    // If process is terminated, restart it
    if (p->state == PROCESS_STATE_TERMINATED) {
        int32_t rc = process_restart(pid);
        if (rc < 0) {
            serial_puts("run: failed to restart process\n");
            return;
        }
        serial_puts("run: restarted terminated process\n");
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

void kmain(void) {
    char input[MAX_INPUT];
    int pos = 0;

    /* Initialize hardware */
    serial_init();

    /* Initialize subsystems */
    memory_init((uint32_t)&__kernel_end);
    process_init();
    scheduler_init(SCHEDULER_DEFAULT_QUANTUM);
    interrupt_init();
    timer_init(100);  // 100 Hz = 10ms per tick

    /* Create ready processes */
    process_create("p1", proc_p1, NULL, 0);
    process_create("p2", proc_p2, NULL, 0);
    process_create("p3", proc_p3, NULL, 0);
    process_create("p4", proc_p4, NULL, 0);
    process_create("p5", proc_p5, NULL, 0);

    /* ================= WELCOME MESSAGE ================= */
    serial_puts("========================================\n");
    serial_puts("    kacchiOS - Minimal Baremetal OS\n");
    serial_puts("========================================\n");
    serial_puts("Hello from kacchiOS!\n");
    serial_puts("Running null process...\n\n");
    serial_puts("Commands: ps | run PID | runq | kill PID\n\n");

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
            } else {
                serial_puts("Unknown command. Try: ps | run PID | runq | kill PID\n");
            }
        }
    }

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
