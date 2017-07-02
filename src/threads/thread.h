/*************************************************************************
	> File Name: thread.h
	> Author: Logan Von
	> Mail: loganwhitevon@gmail.com
	> Created Time: Tue 23 May 2017 08:00:44 PM CST
 ************************************************************************/

#ifndef _THREAD_H
#define _THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>

enum thread_status {
    THREAD_RUNNING,
    THREAD_READY,
    THREAD_BLOCKED,
    THREAD_DYING 
};

/* Thread identifier type */
typedef int tid_t;
#define TID_ERROR ((tid_t)-1)         /* Error value of tid_t */

/* Thread priorities. */
#define PRI_MIN         0             /* Lowest priority */
#define PRI_DEFAULT     31            /* Default priority */
#define PRI_MAX         63            /* Hightest priority */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */

    typedef struct _tag_thread {
        tid_t tid;                  /* Thread identifier */
        enum thread_status status;  /* Thread state. */
        char name[16];              /* Name of the thread, debug purpose. */
        uint8_t *statck;            /* Saved stack pointer. */
        int priority;               /* Priority. */
        struct list_elem all_elem;  /* List element for all threads list. */

        struct list_elem elem;      /* List element. Shared between thread.c and synch.c */
        
#ifdef USERPROG
        /* Owned by userprog/process.c */
        uint32_t *pagedir;          /* Page directory. */
#endif
        /* Owned by thread.c */
        unsigned magic;             /* Detects stack overflows. */

    } thread;

/* If false (Default), use round-robin scheduler. if true, use multi-level
 * feedback queue scheduler. Controlled by kernel command-line opetion "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char* name, int priority, thread_func*, void*);

void thread_block(void);
void thread_unblock(thread*);

thread* thread_current(void);
tid_t thread_tid(void);
const char* thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

/* Performs some operation on thread_t, given auxiliary data AUX. */
typedef void thread_action_func(thread* t, void *aux);
void thread_foreach(thread_action_func*, void*);

int thread_get_priority(void);
void thread_set_priority(void);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

#endif /* End of thread.h */
