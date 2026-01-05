#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "types.h"

// IDT entry structure
typedef struct {
    uint16_t offset_low;    // Offset bits 0-15
    uint16_t selector;      // Code segment selector
    uint8_t zero;           // Always 0
    uint8_t type_attr;      // Type and attributes
    uint16_t offset_high;   // Offset bits 16-31
} __attribute__((packed)) idt_entry_t;

// IDT pointer structure
typedef struct {
    uint16_t limit;         // Size of IDT - 1
    uint32_t base;          // Base address of IDT
} __attribute__((packed)) idt_ptr_t;

// Initialize interrupt system
void interrupt_init(void);

// Enable/disable interrupts
static inline void enable_interrupts(void) {
    __asm__ volatile("sti");
}

static inline void disable_interrupts(void) {
    __asm__ volatile("cli");
}

// Timer functions
void timer_init(uint32_t frequency_hz);
uint32_t timer_get_ticks(void);

#endif
