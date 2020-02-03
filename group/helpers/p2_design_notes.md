Group 75
========

Task 1
------

Correctness:

- Since `strtok_r` inserts null terminators after each token, you should use
  `memcpy` for pushing the arguments string onto the stack

Reminders/Recommendations:

- You probably need to call `strtok_r` before `load` tries to open the file in
  order to insert a null terminator after the file name; since the file name is
  stored as the first argument, the current skeleton will incorrectly load the
  entire string which contains the arguments as well as the file name.

Task 2
------

Correctness:

- In `process_execute` you must ensure that the parent does not return before
  the child finishes loading; this is to return -1 and deallocate structures if
  creation of the process failed
    - An easy way to do this is to have the parent down a semaphore after
      creating a process (or if you want, you can even use the existing
      semaphore in your shared structure) and have the child up it after it
      finishes or fails loading
- You should add process cleanup logic to `process_exit`.
    - `thread_exit` calls `process_exit`, so you can just call `thread_exit`
      whenever you need to kill or exit a process.
- Pointer validation is complicated; not only do you have to ensure that
  the stack pointers to every argument are valid, you also have to ensure that
  every byte in any pointers that you may access later are also valid. In
  practice, this means the validation process can be different for each
  syscall.

Reminders/Recommendations:

- After a `wait` you should always be able to deallocate the struct
- It's probably not necessary to make `child_entry`. Just put the implementation
  in `start_process`

Task 3
------

Correctness:

- Your fd table should look like `struct file **fds`
- In `close`, you should close the file and free the handle for later use
- Remember to close all file descriptors on process exit and free the descriptor
  table
- `file_deny_write` stops working after calling `close` on the file. The current
  skeleton code closes the executable after loading it; you must come up with a 
  scheme to keep the executable open until the process exits
    - Easiest way is to keep a pointer to the open executable file in the thread
      struct somewhere
- As before, must validate entire buffers passed into syscalls (most notably `read`
  and `write`)
- You must lock filesys operations on the executable file (for example, in `load`)

Reminders/Recommendations:

- Remember to release the filesys lock if you're holding it and it turns out you
  need to exit from an invalid pointer.

Additional Questions
--------------------

- #1: sc-bad-sp - it calls assembly that does a syscall on the stack moved down many bytes,
  which should be invalid
- GDB Question: The answer to the last question is that you haven't implemented argument passing yet,
  so the initial location of the stack pointer does not point to valid user memory.
