.text
.globl isr_timer
isr_timer:
    pushal
    pushl   %ds
    pushl   %es
    pushl   %fs
    pushl   %gs

    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs

    call    timer_handler

    popl    %gs
    popl    %fs
    popl    %es
    popl    %ds
    popal

    iret
