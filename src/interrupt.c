#include "interrupt.h"
#include "serial.h"
#include "scheduler.h"

#define IDT_ENTRIES 256
#define PIT_FREQUENCY 1193182
#define IRQ_TIMER 0

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

static volatile uint32_t timer_ticks = 0;

extern void isr_timer(void);

static void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags) {
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
    idt[num].offset_high = (handler >> 16) & 0xFFFF;
}

void interrupt_init(void) {
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }
    idt_set_gate(0x20, (uint32_t)isr_timer, 0x08, 0x8E);
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint32_t)&idt;
    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
}

static void pic_remap(void) {
    __asm__ volatile("outb %0, $0x20" : : "a"((uint8_t)0x11));
    __asm__ volatile("outb %0, $0xA0" : : "a"((uint8_t)0x11));
    __asm__ volatile("outb %0, $0x21" : : "a"((uint8_t)0x20));
    __asm__ volatile("outb %0, $0xA1" : : "a"((uint8_t)0x28));
    __asm__ volatile("outb %0, $0x21" : : "a"((uint8_t)0x04));
    __asm__ volatile("outb %0, $0xA1" : : "a"((uint8_t)0x02));
    __asm__ volatile("outb %0, $0x21" : : "a"((uint8_t)0x01));
    __asm__ volatile("outb %0, $0xA1" : : "a"((uint8_t)0x01));
    __asm__ volatile("outb %0, $0x21" : : "a"((uint8_t)0xFE));
    __asm__ volatile("outb %0, $0xA1" : : "a"((uint8_t)0xFF));
}

void timer_init(uint32_t frequency_hz) {
    pic_remap();
    uint32_t divisor = PIT_FREQUENCY / frequency_hz;
    __asm__ volatile("outb %0, $0x43" : : "a"((uint8_t)0x36));
    __asm__ volatile("outb %0, $0x40" : : "a"((uint8_t)(divisor & 0xFF)));
    __asm__ volatile("outb %0, $0x40" : : "a"((uint8_t)((divisor >> 8) & 0xFF)));
}

uint32_t timer_get_ticks(void) {
    return timer_ticks;
}

void timer_handler(void) {
    timer_ticks++;
    scheduler_on_tick();
    __asm__ volatile("outb %0, $0x20" : : "a"((uint8_t)0x20));
}
