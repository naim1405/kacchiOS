/* context.S - Context switching routines (i386 assembly) */

.text
.globl ctxsw

/*
 * ctxsw - Context switch between processes
 * 
 * C prototype: void ctxsw(void** old_sp, void** new_sp);
 * 
 * Arguments:
 *   old_sp - pointer to location where to save current stack pointer (can be NULL)
 *   new_sp - pointer to location containing stack pointer to restore
 * 
 * This function:
 * 1. Saves current CPU state (registers) to stack
 * 2. Saves stack pointer to *old_sp
 * 3. Loads stack pointer from *new_sp
 * 4. Restores CPU state from new stack
 * 5. Returns (effectively resuming the new process)
 */
ctxsw:
    /* Save callee-saved registers onto current stack */
    pushl   %ebp
    pushl   %ebx
    pushl   %esi
    pushl   %edi
    
    /* Get arguments from stack */
    movl    20(%esp), %eax      /* old_ctx (after 4 pushes = 16 bytes + ret addr = 20) */
    movl    24(%esp), %edx      /* new_ctx */
    
    /* Save old stack pointer if old_ctx is not NULL */
    testl   %eax, %eax
    jz      skip_save
    movl    %esp, (%eax)        /* Save current ESP to *old_ctx */
    
skip_save:
    /* Switch to new stack - load ESP from *new_ctx */
    movl    (%edx), %esp        /* Load new ESP from *new_ctx */
    
    /* Restore callee-saved registers from new stack */
    popl    %edi
    popl    %esi
    popl    %ebx
    popl    %ebp
    
    /* Return (pops return address and jumps to it) */
    ret

/*
 * start_process - Start first process from kernel (one-way jump)
 * 
 * C prototype: void start_process(void** sp);
 * 
 * Arguments:
 *   sp - pointer to location containing stack pointer to restore
 * 
 * This function does NOT return - it jumps to the process
 */
.globl start_process
start_process:
    /* Get argument */
    movl    4(%esp), %eax       /* ctx */
    
    /* Load stack pointer from *ctx */
    movl    (%eax), %esp
    
    /* Restore registers from stack */
    popl    %edi
    popl    %esi
    popl    %ebx
    popl    %ebp
    
    /* Return (jump to saved EIP) */
    ret

/*
 * ctxsw_initial - Initial context switch to a new process
 * Used when starting a process for the first time
 */
.globl ctxsw_initial
ctxsw_initial:
    /* New process starts here after first context switch */
    /* The stack has been set up with: entry function address, arg */
    
    popl    %eax                /* Get entry function address */
    popl    %ecx                /* Get argument */
    
    pushl   %ecx                /* Push arg for function call */
    call    *%eax               /* Call entry(arg) */
    
    /* If process returns, terminate it */
    addl    $4, %esp            /* Clean up arg */
    pushl   $0                  /* exit_code = 0 */
    pushl   %eax                /* Get current PID (we'll need a helper) */
    call    process_exit_current
    
    /* Should never reach here */
1:
    hlt
    jmp     1b

/*
 * Helper function to exit current process
 */
.globl process_exit_current
process_exit_current:
    /* This will be implemented in C */
    ret
