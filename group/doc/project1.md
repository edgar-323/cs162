Design Document for Project 1: Threads
======================================

## Group Members

* Angel Najera <najera_angel@berkeley.edu>
* Edgar Barboza Nunez <barboza_edgar@berkeley.edu>
* Johnny Huang  <johnny.h1997@berkeley.edu>
* William Walker <wewalker@berkeley.edu>

## **Task 1: Efficient Alarm Clock**

### Data Structures and Functions

###### timer.h

```C
/* Sleeping thread structure. */
struct sleep_thread
  {
    /* Pointer to the thread that is sleeping. */
    struct thread *t;

    /* Global tick after which we can wake up. */
    int64_t wakeup;
  };
```

###### timer.c

```C
/* List of currently sleeping threads, sorted
 * in order of earliest to wake up. */
struct list sleep_threads;

/* Modify to add the thread to the kernel sleep list, and then yield to the scheduler.
 * Also, have it turns off interrupts so that we can get to the scheduler safely. */
void timer_sleep (int64_t ticks);

/* Modify to wake up threads that need to be woken
 * up, and yield to the scheduler if we need to. */
static void timer_interrupt (struct intr_frame *args UNUSED);
```

### Algorithms

We will modify `timer_sleep()` so that when called, we turn off interrupts. We will then add the current thread to `sleep_threads`, and call `thread_block()` to block the thread until we are woken up.

Every tick, inside the timer interrupt, we will iterate down the sorted list of sleeping threads, waking up threads that we can and removing them from the list. We stop searching once we get to something that is past the current tick, since we are going over a sorted list. This also keeps the algorithm fast. (important since we are in a interrupt-free context)

If any of the threads that we wake up are of higher priority than the currently running thread, we call `intr_yield_on_return()` to go to the scheduler after we have ticked.

### Synchronization

We believe that going to sleep should be an atomic process. To that end, we will disable interrupts at the beginning of `timer_sleep()` so that we can go to sleep immediately and go to the scheduler.

In addition, since sleeping threads cannot do anything, including exit, `sleep_threads` will never contain a reference to an already dead thread.

### Rationale

Fundamentally, for a thread to not 'busy wait', we cannot have it be keeping track of its own sleep time. Therefore, it must be kept track of in the kernel. While this does add some level of overhead, it is much less so than busy waiting.

Originally, we had the waking-up procedure happen in the `schedule()` function, but this would not guarantee that higher-priority threads that wake up would immediately be scheduled (they would wait for the next call of `schedule()`.)

## **Task 2: Priority Scheduler**

### Data Structures and Functions

###### thread.h
```C
struct thread
  {
    ...
    int base_priority;
    int effective_priority;

    /* Keeps track of the donated priority from the locks the thread
     * owns. Sorted from highest to lowest donation. */
    struct list donations;

    /* If this value is a valid tid, it means that the thread is waiting
     * on the lock currently held by that tid, so we must donate priority
     * to it. */
    struct thread *b_locker;
    ...
  };

/* This struct keeps track of the priority being donated
 * to the thread by a given lock. When we release the lock,
 * we will remove this entry from the list. */
struct donation
  {
    int prior;
    struct lock *lock;
  };

```

###### thread.c
```C
/* Unsorted list of all ready threads. */
struct list ready;

/* Modified to choose based on effective priority. */
static struct thread *next_thread_to_run (void);

/* Modified to get effective priority. */
int thread_get_priority (void);

/* Modified to set the base priority, and atomically update effective priority if need be. */
void thread_set_priority (int new_priority);

/* Modified to put the new thread onto the ready queue. */
tid_t thread_create (const char *name, int priority, thread_func *function, void *aux);

/* Modified to remove the thread from the ready queue, so that it no longer gets scheduled. */
void thread_block (void);

/* Modified to put the thread back onto the ready queue so it can be scheduled. */
void thread_unblock (struct thread *t);

/* Modified to remove the thread from the ready queue, so that it no longer gets scheduled. */
void thread_exit (void);

/* ~~~~~ New functions ~~~~~  */

/* Sets effective priority, and pushes it to the back of the ready queue.
 * Ensure interrupts are off so that we do this atomically.
 * If we lower our own priority, yield to the scheduler. */
void thread_set_eff_priority (int new_priority);

/* Donates t's effective priority to the thread that it depends on, if any,
 * recursing downwards if necessary. */
void thread_donate_priority (struct thread *t, struct lock *lock);
```

###### synchro.c
```C
/* Modified to get rid of the lock's priority donation on the thread that releases
 * the lock, and point all remaining threads' priority donations onto the new thread. */
void lock_release (struct lock *lock);

/* Modified so that if we are blocked, we donate priority
 * to the thread that's inside the lock. */
void lock_acquire (struct lock *lock);

/* Modified to wake up the highest-priority thread. */
void sema_up (struct semaphore *sema);

/* Modified to signal the highest-priority thread. */
void cond_signal (struct condition *cond, struct lock *lock UNUSED);
```

### Algorithms
When the scheduler calls `next_thread_to_run()` to pick the next thread, we call `get_max()` to get the highest priority thread. To enforce round-robin ordering, we will then move that thread to the back of the ready queue.

- Also, since we only ever add ready threads to the queue, we don't have to check for blocked threads. Finding the first maximum element is sufficient.

In terms of synchronizaiton primitives, we need to change only two functions: `sema_up()` and `cond_signal()`, to make sure that they pick the thread with the highest **effective** priority to wake up/signal. We will do this by calling `list_max()` on the list of waiters.

Whether or not to enable priority donation will be decided based on the value of `thread_mlfqs`. Notably, it will be **enabled** when MLFQS is **disabled**.

If priority donation is enabled:

- If a thread is blocked by a lock, we will update the thread's `b_locker` member to point to the blocking thread, and call `thread_donate_priority()` to donate the thread's current **effective** priority to the thread currently in the lock.

  - Donation is a multi-step process. First of all, the donating thread will update the thread's `donations` list entry for the current lock, if we can increase it. We of course make sure that the list stays sorted.

  - Then, we check if we need to modify the thread's effective priority. This happens if the new donated priority is higher than the thread's current effective priority.

  - If we changed the thread's effective priority, we must chain down to the thread that it is waiting on, if there is one. This effectively implements nested donation.

- When a thread successfully acquires a lock, we create an entry in `donations` relating to that lock, and have all the threads still waiting on the lock donate their priority to the thread. Also, we delete the thread's `b_locker` member as it is no longer blocked.

- When a lock is released, we remove the lock's donation to the old thread, and update its effective priority if necessary.

  - We must lower the thread's effective priority if the value we removed from the list was equal to the effective priority, and the other donations and the base priority of the thread are all below the old value.

  - We then update the thread's effective priority to the maximum remaining donation, or the base priority, whichever is higher.


We will also modify `thread_set_priority()` to just change the thread's `base_priority`, and update `effective_priority` if necessary.

- This must be done atomically so as to make sure that our priority is actually changed before the next `schedule()` call.

We will modify `thread_get_priority()` to return the `effective priority` struct member - this means it will take into account priority donations.

At the end of all priority changes (at the bottom of nested donations), we will yield to the scheduler to ensure that we always run the thread with the highest priority.

- Notably, in `thread_set_priority()`, we will only yield if we **reduced** our own priority, since that is the only case in which we may no longer be the highest priority thread.

### Synchronization

Notably, we were told that we do not have to consider the case where a thread exits without releasing a lock. This means that whenever a thread exits, no thread's `b_locker` value will be pointing to it, so we don't have to remove those to avoid memory corruption.

Also, for priority donation which happens inside locks, we don't need to worry about being preempted while changing threads' priority due to nested priority.

- This notably isn't the case when we call `thread_set_priority()`, so we must be sure to turn off interrupts at the beginning of this function.

### Rationale
Ultimately there are two major parts to this problem:
- Priority scheduling
- Priority donation due to locks

Priority scheduling is fairly easy, and there aren't many things to consider. You simply have to be careful that only ready threads are put onto the ready queue, and that they are managed properly.

Originally, we thought it'd be better to use sorted lists to handle synchronization primitives, but we decided it'd be a lot simpler to just get the highest-priority element from an unsorted list (small lists mean sorting is unnecessary.)

In terms of priority donation, we just have to be careful about when we donate priority. Notably, it can almost all be handled in the lock-related functions, as priority donation only makes sense for locks anyway. This also ensures that these donations are atomic (locks disable interrupts).

However, there is an exception. When we change priority due to `thread_set_priority()`, we may not be in an interrupt-free context.

- Because changing priority involves interacting with many core scheduling structures (most notably the ready queue), we **must** run this in an interrupt-free context so as not to be preempted.

- Even if we were preempted in a location that didn't break things outright, when we go to the scheduler the next time, we want to have the updated value of priority for the thread, so we have another reason to make this atomic.

We considered keeping a separate set of variables to determine when we had to yield due to priority changes, but we decided it would be more consistent to just yield to the scheduler instead.


## **Task 3: MLFQS Scheduler**

*All of the implementation in this section will be predicated on the value of `thread_mlfqs` in `thread.c`*

### Data Structures and Functions

###### thread.h

```C
struct thread
  {
    ...
    int niceness;
    fixed_point_t rec_cpu;
    ...
  };
```

###### thread.c

```C
/* Global Variables */
int ready_threads;
fixed_point_t load_avg;

/* Ignore calls to this function when in MLFQS mode. */
void thread_set_priority (int new_priority);

/* Modified to ignore the priority argument, and make the thread have a niceness value of 0. */
tid_t thread_create (const char *name, int priority, thread_func *function, void *aux);

/* Functions to implement */

/* Sets the thread's niceness value, updates
 * its priority, and yields if necessary. */
void thread_set_nice (int nice);

/* Gets the thead's current niceness value. */
int thread_get_nice (void);

/* Returns 100 times the current thread's recent_cpu value. */
int thread_get_recent_cpu (void);

/* Returns 100 times the system load average. */
int thread_get_load_avg (void);
```

###### timer.c

```C
/* Modified to update MLFQS when appropriate. */
timer_interrupt (struct intr_frame *args UNUSED);

/* New functions */

/* Updates priority for all threads every four ticks, and yields to the scheduler. */
void upd_priority (void);

/* Updates rec_cpu for every thread every TIMER_FREQ ticks. */
void upd_rec_cpu (void);

/* Updates the global load_avg every TIMER_FREQ ticks. */
void upd_load_avg (void);
```

We will use the same ready queue structure from problem 2 to implement the priorities, including all related helper functions. Notably:

###### thread.c

```C
/* Unsorted list of all ready threads. */
struct list ready;

/* Sets effective priority, and pushes it to the back of the ready queue.
 * Ensure interrupts are off so that we do this atomically.
 * If we lower our own priority, yield to the scheduler. */
void thread_set_eff_priority (int new_priority);
```

### Algorithms
First of all, since priority donation is predicated on `thread_mlfqs`, priority donation will be disabled when MLFQS is enabled.

We need a new `struct thread` member `niceness` to keep track of the thread's niceness for calculation MLFQS.

- The `thread_get_nice()`will simply return this member.

- The `thread_set_nice()` will set the thread's niceness value, and then **atomically**  update its priority.
  - If it was reduced, yield to the scheduler to ensure that it is still the highest priority thread.

We will modify the `timer_interrupt` function in `timer.c` to update the relevent MLFQS values when needed. Specifically:

- Every tick, for the currently running thread, increment `rec_cpu`.

  - This will not require a function, since it's just a simple incrementation.


- Every four ticks, for every thread, update its priority using a new function `upd_priority`.

  - This will yield to the scheduler to ensure the highest priority thread is running.


- Every `TIMER_FREQ` ticks (every second), we update the `rec_cpu` for all threads, and the global `load_avg`.

Because we will be using the same ready queue structure as in Q2, we will enforce round-robin ordering here as well.

### Synchronization
Fundamentally, since all of the MLFQS calculations occur on timer tick intervals, they should be handled in the timer interrupt handler, so we do not need to worry about synchronization. (Interrupts are disabled in interrupt handlers)

However, when a thread sets its niceness value with `thread_set_nice()`, we must atomically update the thread's priority to ensure we aren't preempted.

- If we didn't do this, we could be preempted in the middle of updating priority, and the thread would need to be scheduled with its old priority value to finish. Thus, all priority-setting must be done in an interrupt-free context.

### Rationale
Ultimately, this question is more centered around getting the MLFQS algorithms correct than about particular design decisions.

However, there was again the issue of whether or not to turn off interrupts when changing priority (due to `thread_set_nice()`). We made the same decision as before: because changing priority involves interacting with many core scheduling structures, we **must** run this in an interrupt-free context so as not to be preempted.


## **Additional Questions**

### Question 1

In order to test this, we need this situation: (bp = base priority)

```
Lock 1:
Thread 1 (bp = 0) [Owner]
Thread 2 (bp = 1)
Thread 3 (bp = 2)

Lock 2:
Thread 2 (bp = 1) [Owner]
Thread 4 (bp = 3)
```

This means that thread 4 will donate its priority to thread 2, so we will have this situation: (ep = effective priority)

```
Lock 1:
Thread 1 (ep = 0) [Owner]
Thread 2 (ep = 3)
Thread 3 (ep = 2)

Lock 2:
Thread 2 (ep = 3) [Owner]
Thread 4 (ep = 3)
```

At this point, thread 1 should release its lock.

If thread 2 gets into the lock, it will print "Thread 2 made it into the lock!" and kernel panic.
If thread 3 gets into the lock, it will just kernel panic.

This allows us to determine the ordering of which thread gets in first.

Expected behavior: Because thread 2 has higher **effective** priority than thread 3, thread 2 gets in, prints, and panics.

If the bug exists: Because thread 3 has higher **base** priority than thread 2, it gets in, and panics.

Essentially, whether or not the print statement goes off will be sufficient to determine the existence of the bug.


### Question 2

timer ticks | R(A) | R(B) | R(C) | P(A) | P(B) | P(C) | thread to run
------------|------|------|------|------|------|------|--------------
 0          |0     |0     |0     |63    |61    |59    |A
 4          |4     |0     |0     |62    |61    |59    |A
 8          |8     |0     |0     |61    |61    |59    |B
12          |8     |4     |0     |61    |60    |59    |A
16          |12    |4     |0     |60    |60    |59    |B
20          |12    |8     |0     |60    |59    |59    |A
24          |16    |8     |0     |59    |59    |59    |C
28          |16    |8     |4     |59    |59    |58    |B
32          |16    |12    |4     |59    |58    |58    |A
36          |20    |12    |4     |58    |58    |58    |C


### Question 3

The only ambiguity we reached was in the case when two threads tied for priority (e.g. tick 8).

In this case, we can utilize the round-robin behavior of MLFQS to tie break. For instance, from the beginning, thread B was in the bucket of priority 61. Later on, A drops down into that bucket, but since it is tagged onto the end of the list, thread B runs first.

Notably, this means that if a thread moves into an already-occupied bucket, it will always run after all of the threads that were already in that bucket.

