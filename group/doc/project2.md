Design Document for Project 2: User Programs
============================================

## Group Members

* Angel Najera <najera_angel@berkeley.edu>
* Edgar Barboza Nunez <barboza_edgar@berkeley.edu>
* Johnny Huang  <johnny.h1997@berkeley.edu>
* William Walker <wewalker@berkeley.edu>

## **Task 1: Argument Passing**

### Data Structures and Functions

###### process.c

```C
/* Modify the following methods */
static void start_process (void *file_name_);
```

### Algorithms
- Using `strtok_r()`, we tokenize `file_name_`, storing pointers to each token as we go.

- Then, we will `strcpy()` the arguments onto the stack in reverse order, conforming to 80x86 calling convention. 

	- As we go, we'll keep track of the locations of these buffers, for the pointers later.

- Then we push enough null bytes to get to a word-boundary.

- Then a null.

- Then, using our array of pointers, we'll push the pointers to each argument (again in reverse order).

- Then, we'll push our current position + 4, which will point to `argv[0]`.

- Finallly, we push argc and a dummy return address, for which we will use `0xDEADC0DE` for maximum style points.

At the end, free the new page we allocated in `process_execute()` for the arguments.

### Synchronization

As already provided, we will copy `file_name` so that we don't have a data race.

Other than that, there are no other synchronization concerns.

### Rationale

We considered calculating addresses of the `argv` buffers on the fly, but figured that using an array would be faster and easier.


## **Task 2: Process Control Syscalls**

### Data Structures and Functions

###### thread.h

```C
struct thread
  {
    ...
    /* Our status, shared with our parent. */
    wait_status *my_wait_status;
    /* The statuses of our children. */
    struct list children_status;
    ...
  };
```
###### process.h

```C
struct wait_status 
  {
    /* Children list elem. */
    struct list_elem elem;
    /* Protects ref_cnt. */
    struct lock lock;
    /* Number of threads still using. Starts at 2 since parent + child. */
    int ref_cnt;
    /* Child thread ID. */
    tid_t tid;
    /* Used to pass the child's exit code to the parent. */
    int exit_code;
    /* Starts at 0, parent waits on this for wait(), and child sema_up()'s it when it exits. */
    struct semaphore dead;
  };
```

###### process.c

```C
/* Modify the following methods */
tid_t process_execute (const char *file_name);
static void start_process (void *file_name_);

/* This function sets the `my_status` field of the newly created
   child thread to match the parent's, and then calls `start_process()`
   on the proper string. */
static void child_entry (void *data);
```

###### syscall.c

```C
/* Modify the following methods */
static void syscall_handler (struct intr_frame *f UNUSED);

/* Add these functions for handling user pointers: */

/* Reads n bytes from src into dst, killing the process
   if we encounter a page fault, or if any part of the
   user buffer src is above PHYS_BASE. */
static void get_user_n(uint8_t *dst, uint8_t *src, int n);

/* Reads one byte from the given pointer, returning -1 on error. */
static int get_user(const uint8_t *uaddr);

/* Writes n bytes from src into dst, killing the process
   if we encounter a page fault, or if any part of the
   user buffer dst is above PHYS_BASE. */
static void put_user_n(uint8_t *src, uint8_t *dst, int n);

/* Writes one byte to the given position, returning false if a segfault occurs. */
static bool put_user (uint8_t *udst, uint8_t byte);
```

### Algorithms

Regarding memory: all accesses to user-provided pointers will be handled with `get_user_n()` or `put_user_n()` respectively. If either of these calls fail, we will kill the calling process.

**practice** 
 
 Increments the argument and returns it.

**exec**

In `process_execute()`, we will add the following:

- Call `file_deny_write()` to ensure that nobody can modify our executable.

- Create a new `wait_status` struct to share between the parent and child, and initialize it appropriately.

- Add this `wait_status` to the parent's list of children.

We will also modify the arguments of `thread_create()` to make the child start at `child_entry()` instead of `start_process()`.

In addition, instead of passing just the filename to the child, we will also pass a pointer to the shared struct. Specifically, we'll pass `filename || &wait_status`.

  - `child_entry()` will take care of separating out this data.

Finally, after loading the file, we'll call `file_allow_write()` to re-allow writing to the file.

**halt**

Call `shutdown_power_off()`.

**wait** (Inspired by the section 8 worksheet)

- Iterate through `children_status`, looking for the corresponding entry.
  - If none are found, return -1.

- Call `sema_down()` on `dead` to wait until the child is done.

- Obtain the child's exit code (stored in `exit_code`)

- Remove the `wait_status` struct from the list and free it.

- Return the exit code.

**exit** (Inspired by the section 8 worksheet)

For all `wait_status` we have access to (parent and children):
  - Decrement `ref_cnt`, letting the other thread know that we're done. If `ref_cnt` is now 0, free the struct.

- For the parent, store our exit code, and up the semaphore if we aren't freeing the struct.

- Terminate the thread.

### Synchronization

We mantain child-parent synchronization via the lock and semaphore within a `struct wait_status` datastructure. The lock ensures that at most one thread makes changes to the fields of a `wait_status` (e.g., the status code), and the sempahore allows us to check whether the child thread has terminated.  

### Rationale

In order for a parent and child process to hold communication, we must have some sort of data structure that both threads can exclusively access and check. This is the purpose of `stuct wait_status` datastructure.

- Notably, this is somewhat hard to pass to the child, but can be done through tha `aux` field of `thread_create()`.

We decided to go with the page-fault method of checking user addresses, since fast > easy.

## **Task 3: File Operation Syscalls**

### Data Structures and Functions

###### thread.h

```C
struct thread
  {
    ...
    /* File Descriptors */
    uint_32t fd_cap;
    file *fds;
    ...
  };
```

###### syscall.c

```C
/* Global filesys lock. */
struct lock *filesys_lock;
```

### Algorithms

All of these syscalls will acquire `filesys_lock` at their start, and release it at the end, to ensure that we only have one thread in the file system at a time.

In addition, all operations on file descriptors can be assumed to operate on the `file` struct at `fds[fd - 2]`. fds 0 and 1 are handled separately.

**create** 

This will call `filesys_create()` to create a file on the disk with the given name, returning its success value.

**remove** 

This will call `filesys_remove()` to remove the file from the disk, returning its success value.

We have made an assumption regarding how the disk handles allocating (and deallocating) sectors of memory.

- In particular, we assume that the `filesys_remove()` call simply breaks the link between the filename and the sector on the disk, but doesn't deallocate the memory, and any open `file` structs with pointers to that sector can still modify it. And also, when there are no `file` structs pointing to the sector, the disk frees the sector.

**open** 

We will call `file_open` to obtain a `file` struct corresponding to the file. If we fail, return -1 here.

Then we will iterate through `fds` and find the first entry which is NULL, replacing it with the prior `file` struct and returning its index + 2, which will act as the file descriptor.

If there are no valid file descriptors inside the array, call `realloc()` with double the size to give us access to more file descriptors, and update `fd_cap`.

**filesize** 

This will simply return `file_length()`, or error appropriately if the file descriptor isn't valid.

**read** 

Verifies that `buffer` is valid as described above, and then calls the appropriate function:

- If `fd` is invalid or 1, return -1.

- If `fd` is 0, call `input_getc()` in a loop to get data from the keyboard, and put it in the buffer.

- If `fd` is another valid file descriptor, call `file_read()` to copy from the file into the buffer.

**write** 

Verifies that `buffer` is valid, and then calls the appropriate function:

- If `fd` is invalid or 0, return -1.

- If `fd` is 1, call `putbuf()` to print the buffer to the console.

- If `fd` is another valid descriptor, call `file_write()` to write to disk.

**seek** 

This will simply call `file_seek()`, or error appropriately if we don't have a valid descriptor.

**tell** 

This will simply call `file_tell()`, or error appropriately if we don't have a valid descriptor.

**close**

First, we will set the appropriate index of `fds` to `NULL`, and then call `file_close()`.

### Synchronization

Our global lock `filesys_lock` will ensure that only one thread can be inside the file system at a time, so there are no concerns regarding synchronization.

### Rationale

In essence, this section is almost entirely implemented already in `filesys/`, and all we have to do is call the library functions.

One interesting decision was how to handle file descriptors. An array that holds `file` pointers is effectively a map between integers and `file` structs, and handles the problem beautifully.

## **Design Document Additional Questions**

### Question 1

Test case: `child-bad.c`. This program trashes the stack pointer (sets it to `0x20101234`) and then executes a system call on line 12, which should make the syscall kill the program.

### Question 2

`sc-bad-arg.c`: The test case sets a system call number (`SYS_EXIT`) to the top of the stack at line 14:

```assembly
movl $0xbffffffc, %%esp
```

However, when the syscall is executed, we error because the argument for the syscall will be above the boundary.

### Question 3

File management is not fully tested. There are simple tests that close and open a file, however, there is no test that checks for valid read/writes of a removed file which still hold other references to it.

To test this, two different threads open the the same file, then one of the threads removes it, and we ensure that the second thread can still read/write from the file until it closes it (or removes it). 

Another test we can add is having two threads open same file, then having one of them sleep, while the other thread calls `remove` on the file, then a third thread can call `create` and `open` on the old filename. This test is stricter but must also succeed.

## **GDB Questions**

### Question 1

**Current Thread**

`thread name: main`

`thread address: 0xc000e00`

```
{
  tid = 1, 
  status = THREAD_BLOCKED, 
  name = "main", '\000' <repeats 11 times>, 
  stack = 0xc000eebc "\001", 
  priority = 31, 
  effective_priority = 31, 
  donations = {head = {prev = 0x0, next = 0xc000e02c}, tail = {prev = 0xc000e024, next = 0x0}}, 
  b_locker = 0x0, 
  niceness = 0, 
  rec_cpu = {f = 0}, 
  allelem = {prev = 0xc0035e10 <all_list>, next = 0xc0104040}, 
  wakeup = 76, 
  elem = {prev = 0xc0037814 <temporary+4>, next = 0xc003781c <temporary+12>}, 
  pagedir = 0x0, 
  magic = 3446325067
}
```

**Other Threads**

`thread name: idle`

`thread address: 0xc0104000`

```
{
  tid = 2, 
  status = THREAD_READY, 
  name = "idle", '\000' <repeats 11 times>, 
  stack = 0xc0104eb8 "", 
  priority = 0, 
  effective_priority = 0, 
  donations = {head = {prev = 0x0, next = 0xc010402c}, tail = {prev = 0xc0104024, next = 0x0}}, 
  b_locker = 0x0, 
  niceness = 0, 
  rec_cpu = {f = 0}, 
  allelem = {prev = 0xc000e040, next = 0xc0035e18 <all_list+8>}, 
  wakeup = -1, 
  elem = {prev = 0xc0035e20 <ready_list>, next = 0xc0035e28 <ready_list+8>}, 
  pagedir = 0x0, 
  magic = 3446325067
}
```

### Question 2

\#0  `process_execute (file_name=file_name@entry=0xc0007d50 "args-none") at ../../userprog/process.c:32`

\#1  `0xc002025e in run_task (argv=0xc0035ccc <argv+12>) at ../../threads/init.c:285`

\#2  `0xc00208e4 in run_actions (argv=0xc0035ccc <argv+12>) at ../../threads/init.c:337`

\#3  `main () at ../../threads/init.c:131`

**C Code**

\#0 `process_execute (const char *file_name)` 

\#1 `process_wait (process_execute (task));`

\#2 `a->function (argv);`

\#3 `run_actions (argv);`

### Question 3

`thread name: args-none\000\000\000\000\000\000`

`thread address: 0xc010a000`

```
{
  tid = 3, 
  status = THREAD_RUNNING, 
  name = "args-none\000\000\000\000\000\000", 
  stack = 0xc010afd4 "", 
  priority = 31, 
  effective_priority = 31, 
  donations = {head = {prev = 0x0, next = 0xc010a02c}, tail = {prev = 0xc010a024, next = 0x0}}, 
  b_locker = 0x0, 
  niceness = 0, 
  rec_cpu = {f = 0}, 
  allelem = {prev = 0xc0104040, next = 0xc0035e18 <all_list+8>}, 
  wakeup = -1, 
  elem = {prev = 0xc0035e20 <ready_list>, next = 0xc0035e28 <ready_list+8>}, 
  pagedir = 0x0, 
  magic = 3446325067
}
```

**Other Threads**

```
  {
    tid = 1, 
    status = THREAD_BLOCKED, 
    name = "main", '\000' <repeats 11 times>, 
    stack = 0xc000eebc "\001", 
    priority = 31, 
    effective_priority = 31, 
    donations = {head = {prev = 0x0, next = 0xc000e02c}, tail = {prev = 0xc000e024, next = 0x0}}, 
    b_locker = 0x0, 
    niceness = 0, 
    rec_cpu = {f = 0}, 
    allelem = {prev = 0xc0035e10 <all_list>, next = 0xc0104040}, 
    wakeup = 76, 
    elem = {prev = 0xc0037814 <temporary+4>, next = 0xc003781c <temporary+12>}, 
    pagedir = 0x0, 
    magic = 3446325067
  }
```
```
  {
    tid = 2, 
    status = THREAD_READY, 
    name = "idle", '\000' <repeats 11 times>, 
    stack = 0xc0104eb8 "",
    priority = 0, 
    effective_priority = 0, 
    donations = {head = {prev = 0x0, next = 0xc010402c}, tail = {prev = 0xc0104024, next = 0x0}}, 
    b_locker = 0x0, 
    niceness = 0, 
    rec_cpu = {f = 0}, 
    allelem = {prev = 0xc000e040, next = 0xc010a040}, 
    wakeup = -1, 
    elem = {prev = 0xc0035e20 <ready_list>, next = 0xc0035e28 <ready_list+8>}, 
    pagedir = 0x0, 
    magic = 3446325067
  }
```

### Question 4
`0xc00218cb in kernel_thread (function=0xc002ad95 <start_process>, aux=0xc0109000) at ../../threads/thread.c:485`

`thread.c:485 - function (aux);`

### Question 5
0x0804870c

### Question 6
`_start (argc=<error reading variable: cant compute CFA for this frame>, argv = <error reading variable: cant compute CFA for this frame>) at ../../lib/user/entry.c:9 /"`

### Question 7
A user program can only access its own user virtual memory. Thus, we are page faulting because we are trying to access kernel virtual memory.
