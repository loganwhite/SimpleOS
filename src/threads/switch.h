/*************************************************************************
	> File Name: switch.h
	> Author: Logan Von
	> Mail: loganwhitevon@gmail.com
	> Created Time: Tue 23 May 2017 08:53:37 PM CST
 ************************************************************************/

#ifndef _SWITCH_H
#define _SWITCH_H

#ifndef __ASSEMBLER__
/* switch_thread()'s stack frame. */
struct switch_threads_frame {
    uint32_t edi;           /* 0: saved %edi. */
    uint32_t esi;           /* 4: saved %esi. */
    uint32_t ebp;           /* 8: saved %ebp. */
    uint32_t ebx;           /* 12: saved %ebx. */
    void (*eip) (void);     /* 16: Return address. */
    struct thread* cur;     /* 20: switch_threads()'s cur argument.' */
    struct thread* next;    /* 24: switch_threads()'s next argument.' */
};

/* Switch from cur to next, and must be running thread. And which must be
   also running switch_threads(), returning cur in Next's context. */

struct thread* switch_threads(struct thread* cur, struct thread* next);

/* Stack frame for switch_entry(). */
struct switch_entry_frame() {
    void (*eip) (void);
};

void switch_entry(void);

/* Pops the CUR and NEXT arguments off the stack, 
   for use in initializing threads. */
void switch_thunk(void);
#endif  /* End of __ASSEMBLER__ */

/* Offset used by switch.S. */
#define SWITCH_CUR      20
#define SWITCH_NEXT     24


#endif  /* End of switch.h */
