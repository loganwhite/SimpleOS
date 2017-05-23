#include "threads/switch.h"

#### struct thread* switch_thread (struct thread* cur, struct thread* next);
####
#### Switches from CUR, which must be the running thread, to NEXT, which must alse
#### be running switch_threads(), returning CUR in NEXT's context.

#### This function works by assuming that the thread we're switching into is also
#### running switch_threads(). Thus, all it has to do is preserve a few registers.
#### As part of switching stacks we record the current statck pointer in CUR's
#### thread structure.

.globl switch_threads
.func switch_threads
switch_threads:
    ## Save caller's register state.
    ## Note that the SVR4 ABI allows us to destroy %eax, %ecx, %edx, but requires
    ## us to preserve %ebx, %ebp, %esi, %edi.

    ## This stack frame must match the one set up by thread_create() in size.

    pushl %ebx
    pushl %ebp
    pushl %esi
    pushl %edi

    ## Get offsetof (struct thread, statck).
.globl thread_stack_ofs
    mov thread_stack_ofs, %edx

    ## save current stack pointer to old thread's stack, if any.
    movl SWITCH_CUR(%esp), %eax
    movl %esp, (%eax,%edx,1)

    ## restore stack pointer from new thread's stack.
    movl SWITCH_NEX(%esp), %ecx
    movl (%ecx, %edx, 1), %esp

    ## restore caller's register state.
    popl %edi
    popl %esi
    popl %ebp
    popl %ebx
    
    ret
.endfunc

.globl switch_entry
.func switch_entry
switch_entry:
    ## Discard switch_threads() arguments.
    addl $8, %esp

    ## Call thread_schedule_tail(prev).
    pushl %eax
.globl thread_schedule_tail
    call thread_schedule_tail
    addl $4, %esp

    ## start thread proper.
    ret
.endfunc
