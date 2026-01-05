#include "interrupt.h"
#include "serial.h"
#include "src/scheduler.h"

#define IDT_ENTRIES 256
#define PIT_FREQUENCY 1193182  // PIT base frequency in Hz
#define IRQ_TIMER 0

// IDT table
static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

// Timer state
static volatile uint32_t timer_ticks = 0;

// External assembly interrupt handlers
extern void isr_timer(void);

// Set an IDT entry
static void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags) {
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
    idt[num].offset_high = (handler >> 16) & 0xFFFF;
}

// Initialize IDT
void interrupt_init(void) {
    // Clear IDT
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }
    
    // Set up timer interrupt (IRQ0 = INT 0x20)
    // Flags: 0x8E = present, DPL=0, 32-bit interrupt gate
    idt_set_gate(0x20, (uint32_t)isr_timer, 0x08, 0x8E);
    
    // Load IDT
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint32_t)&idt;
    
    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
}

// PIC (Programmable Interrupt Controller) helper functions
static void pic_remap(void) {
    // Remap PIC IRQs from 0-15 to 0x20-0x2F
    // This avoids conflict with CPU exceptions
    
    // ICW1: Start initialization
    __asm__ volatile("outb %0, $0x20" : : "a"((uint8_t)0x11));
    __asm__ volatile("outb %0, $0xA0" : : "a"((uint8_t)0x11));
    
    // ICW2: Set IRQ base addresses
    __asm__ volatile("outb %0, $0x21" : : "a"((uint8_t)0x20));  // Master: 0x20-0x27
    __asm__ volatile("outb %0, $0xA1" : : "a"((uint8_t)0x28));  // Slave: 0x28-0x2F
    
    // ICW3: Master/slave relationship
    __asm__ volatile("outb %0, $0x21" : : "a"((uint8_t)0x04));
    __asm__ volatile("outb %0, $0xA1" : : "a"((uint8_t)0x02));
    
    // ICW4: 8086 mode
    __asm__ volatile("outb %0, $0x21" : : "a"((uint8_t)0x01));
    __asm__ volatile("outb %0, $0xA1" : : "a"((uint8_t)0x01));
    
    // Mask all IRQs except timer (IRQ0)
    __asm__ volatile("outb %0, $0x21" : : "a"((uint8_t)0xFE));  // Unmask IRQ0
    __asm__ volatile("outb %0, $0xA1" : : "a"((uint8_t)0xFF));  // Mask all slave IRQs
}

// Initialize PIT timer
void timer_init(uint32_t frequency_hz) {
    pic_remap();
    
    // Calculate divisor for desired frequency
    uint32_t divisor = PIT_FREQUENCY / frequency_hz;
    
    // Send command byte: channel 0, lobyte/hibyte, rate generator
    __asm__ volatile("outb %0, $0x43" : : "a"((uint8_t)0x36));
    
    // Send divisor
    __asm__ volatile("outb %0, $0x40" : : "a"((uint8_t)(divisor & 0xFF)));
    __asm__ volatile("outb %0, $0x40" : : "a"((uint8_t)((divisor >> 8) & 0xFF)));
}

uint32_t timer_get_ticks(void) {
    return timer_ticks;
}

// Timer interrupt handler (called from assembly)
// Timer tracks time but processes must cooperatively check and yield
void timer_handler(void) {
    timer_ticks++;
    
    // Call scheduler tick to update timing
    scheduler_on_tick();
    
    // Note: Can't directly context switch from interrupt
    // Processes check scheduler_should_switch() and call process_yield()
    
    // Send EOI (End of Interrupt) to PIC
    __asm__ volatile("outb %0, $0x20" : : "a"((uint8_t)0x20));
}
