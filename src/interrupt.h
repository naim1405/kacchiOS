#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "types.h"

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

void interrupt_init(void);

static inline void enable_interrupts(void) {
    __asm__ volatile("sti");
}

static inline void disable_interrupts(void) {
    __asm__ volatile("cli");
}

void timer_init(uint32_t frequency_hz);
uint32_t timer_get_ticks(void);

#endif
