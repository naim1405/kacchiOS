/* interrupt.S - Interrupt handlers */

.text

/*
 * Timer interrupt handler (IRQ0 / INT 0x20)
 * This saves all registers, calls C handler, then restores and returns
 */
.globl isr_timer
isr_timer:
    /* Save all registers */
    pushal          /* Push EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI */
    pushl   %ds
    pushl   %es
    pushl   %fs
    pushl   %gs
    
    /* Load kernel data segment */
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    
    /* Call C handler */
    call    timer_handler
    
    /* Restore all registers */
    popl    %gs
    popl    %fs
    popl    %es
    popl    %ds
    popal
    
    /* Return from interrupt */
    iret
