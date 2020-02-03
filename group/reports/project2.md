Final Report for Project 2: User Programs
=========================================

### Changes

- For task 1, we did not make any big changes in terms of algorithm design. Only change made was using `memcpy()` instead of `strcopy()` since we are using `strtok_r()`. This was something that was discussed in our design review.

- For task 2, there were a lot of finer points that had to be addressed pertaining to waiting. For instance, we added an `int exit_code` field to `struct thread` to keep track of a thread's exit status.

	- We also used this to deal with printing out exit code in case of a process dying to a page fault.

	- Modifying exception.c ended up being much much easier than expected.

- For task 3, there was a change in the design of our filesystem handling scheme. To begin, we realized that when reopening a file, we needed to make sure to fetch the same inode and allow it to internally increment its reference count `open_cnt`. So when opening a certain file, we had to check if this file was open before. We did this by introducing `struct FILE` which was a wrapper for `struct file` which also included the name of the file as `char* name`. Furthermore, we kept the list of open file as a global list `struct FILE** fds` and within `struct thread` we added `int* fds` in order to know which position within the global list kept an open file of some thread. This was useful, first, because we were able to check whether a file has been opened before and calling the appropriate file calls (e.g, `file_reopen`), second, because it hid the amount of total open files by the kernel hidden from any user thread (i.e., security concerns) and, third, because it allowed us to know where within the global list we can find the files corresponding to a given thread and close them all when exiting (if the thread did not explicitly call `close` on all its open files).

### Work Distribution

* Johnny: Design Doc, Task 1, Debugging (Task 1), test cases, final report
* Will: Design Doc, Task 2, Debugging (All)
* Edgar: Design Doc, Task 3, Debugging (All)
* Angel: None

### Reflection
Some problems we ran into for the project was planning and working collaboratively. Due to the midterm season, we were not able to work together as much as we would have liked. This lead to some problems when we were debugging as we did not know exactly which parts were causing bugs and such.

Overall, of the three of us who worked on it, we all put in equal commitment into the project. We all worked together on the design doc and the final debugging stage.

Our fourth member, Angel, has been MIA and did not contribute to the project.

## Student Testing Report

### Test 1: read-missing

#### Description
Makes sure that when a file is removed any process that still has the file descriptor for the file can continue to use the file.

#### Overview
We opened a file in the parent process and then called a child process that removes that file. After executing the child process, the parent process should still be able to continue to use the file.

#### Output

###### read-missing.output
```
Copying tests/userprog/read-missing to scratch partition...
Copying ../../tests/userprog/sample.txt to scratch partition...
Copying tests/userprog/child-remove to scratch partition...
qemu -hda /tmp/8fmYQPCj5c.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading...........
Kernel command line: -q -f extract run read-missing
Pintos booting with 4,088 kB RAM...
382 pages available in kernel pool.
382 pages available in user pool.
Calibrating timer...  548,864,000 loops/s.
hda: 5,040 sectors (2 MB), model "QM00001", serial "QEMU HARDDISK"
hda1: 176 sectors (88 kB), Pintos OS kernel (20)
hda2: 4,096 sectors (2 MB), Pintos file system (21)
hda3: 203 sectors (101 kB), Pintos scratch (22)
filesys: using hda2
scratch: using hda3
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'read-missing' into the file system...
Putting 'sample.txt' into the file system...
Putting 'child-remove' into the file system...
Erasing ustar archive...
Executing 'read-missing':
(read-missing) begin
(read-missing) open "sample.txt"
(child-remove) begin
(child-remove) end
child-remove: exit(0)
(read-missing) wait(exec()) = 0
(read-missing) verified contents of "sample.txt"
(read-missing) end
read-missing: exit(0)
Execution of 'read-missing' complete.
Timer: 63 ticks
Thread: 30 idle ticks, 32 kernel ticks, 2 user ticks
hda2 (filesys): 161 reads, 417 writes
hda3 (scratch): 202 reads, 2 writes
Console: 1177 characters output
Keyboard: 0 keys pressed
Exception: 0 page faults
Powering off...
```

###### read-missing.result
```
PASS
```

#### Potential Kernel Bugs
- If the kernel deallocated all the blocks of a file when it is removed, then our test case would fail and get a page fault.

- If the kernel closed all file descriptors when a file is removed, then our test case would fail and get a page fault.
### Test 2: sc-null

#### Description
Makes sure that null stack pointers are properly dealt with.

#### Overview
We set the stack pointer to `NULL` and check to make sure the process exits with exit code -1.

#### Output

###### sc-null.output
```
Copying tests/userprog/sc-null to scratch partition...
qemu -hda /tmp/TnwahZ0NFV.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading...........
Kernel command line: -q -f extract run sc-null
Pintos booting with 4,088 kB RAM...
382 pages available in kernel pool.
382 pages available in user pool.
Calibrating timer...  508,723,200 loops/s.
hda: 5,040 sectors (2 MB), model "QM00001", serial "QEMU HARDDISK"
hda1: 176 sectors (88 kB), Pintos OS kernel (20)
hda2: 4,096 sectors (2 MB), Pintos file system (21)
hda3: 101 sectors (50 kB), Pintos scratch (22)
filesys: using hda2
scratch: using hda3
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'sc-null' into the file system...
Erasing ustar archive...
Executing 'sc-null':
(sc-null) begin
sc-null: exit(-1)
Execution of 'sc-null' complete.
Timer: 64 ticks
Thread: 30 idle ticks, 34 kernel ticks, 1 user ticks
hda2 (filesys): 61 reads, 206 writes
hda3 (scratch): 100 reads, 2 writes
Console: 859 characters output
Keyboard: 0 keys pressed
Exception: 1 page faults
Powering off...
```

###### sc-null.result
```
PASS
```

#### Potential Kernel Bugs
- If the kernel did not print the exit code in `process_exit`, then the test case would not output an exit code since we freed the struct.

- If the kernel changed the exit code to -1 outside of our page fault handler, then the test case would output `exit(0)` instead.
