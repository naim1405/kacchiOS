.text
.globl ctxsw

ctxsw:
    pushl   %ebp
    pushl   %ebx
    pushl   %esi
    pushl   %edi

    movl    20(%esp), %eax
    movl    24(%esp), %edx

    testl   %eax, %eax
    jz      skip_save
    movl    %esp, (%eax)

skip_save:
    movl    (%edx), %esp

    popl    %edi
    popl    %esi
    popl    %ebx
    popl    %ebp

    ret

.globl start_process
start_process:
    movl    4(%esp), %eax
    movl    (%eax), %esp

    popl    %edi
    popl    %esi
    popl    %ebx
    popl    %ebp

    ret

.globl ctxsw_initial
ctxsw_initial:
    popl    %eax
    popl    %ecx

    pushl   %ecx
    call    *%eax

    addl    $4, %esp
    pushl   $0
    pushl   %eax
    call    process_exit_current

1:
    hlt
    jmp     1b

.globl process_exit_current
process_exit_current:
    ret
