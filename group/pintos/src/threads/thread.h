#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include "threads/fixed-point.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

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
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Base Priority. */

    int effective_priority;             /* Effective Priority. */
    struct list donations;              /* List of donated priorities. */
    struct lock *b_locker;              /* Pointer to lock that we're waiting on. */
    int niceness;                       /* Niceness. Used for MLFQS. */
    fixed_point_t rec_cpu;              /* How much CPU time the thread has had recently. */
    struct list_elem allelem;           /* List element for all threads list. */
    int64_t wakeup;                     /* What tick to be woken up on if sleeping. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
    int exit_code;                      /* The exit code of the thread. */
    struct wait_status *my_wait_status; /* Our status, shared with our parent. */
    struct list children_status;        /* The statuses of our children. */
    struct file *source;                /* Source file. Close on exit. */
    /* File Descriptors */
    uint32_t fd_cap;                    /* */
    int* fds;                 /* */
#endif

#ifdef FILESYS
    /* Our working directory. */
    struct dir *cwd;
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/* This struct keeps track of the priority being donated
 * to the thread by a given lock. When we release the lock,
 * we will remove this entry from the list. */
struct donation
  {
    int prior;
    struct lock *lock;
    struct list_elem elem;              /* For the donations list. */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

/* Number of threads on the ready list. */
int ready_threads;

/* System load average. */
fixed_point_t load_avg;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);
extern void upd_priority (struct thread *t, void *aux UNUSED);

/* Helper functions */
bool list_contains (struct list * ll, struct list_elem * e);
bool priority_comparator(const struct list_elem *a,
                             const struct list_elem *b,
                             void *aux);
bool donation_comparator (const struct list_elem * a,
                             const struct list_elem * b,
                             void * aux);
struct thread* max_priortiy_thread(struct list* ll);

/* Sets effective priority, and pushes it to the back of the ready queue.
 * Ensure interrupts are off so that we do this atomically.
 * If we lower our own priority, yield to the scheduler. */
void thread_set_eff_priority (int new_priority);

/* Donates t's effective priority to the thread that it depends on, if any,
 * recursing downwards if necessary. */
void thread_donate_priority (struct thread *t, struct lock *lock);

#endif /* threads/thread.h */
