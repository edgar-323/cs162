#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

struct wait_status
  {
    /* Children list elem. */
    struct list_elem elem;
    /* Protects ref_cnt. */
    struct lock lock;
    /* Number of threads still using. */
    int ref_cnt;
    /* Child thread ID. */
    tid_t tid;
    /* Used to pass the child's exit code to the parent. */
    int exit_code;
    /* Starts at 0, parent waits on this for wait(), and child sema_up()'s it when it exits. */
    struct semaphore dead;
  };

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
