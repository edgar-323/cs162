Final Report for Project 3: File System
=======================================

### Changes

- Task 1: Buffer Cache
	- put changes here ..
- Task 2:
	- put changes here ..
- Task 3: 
	- The algorithm for the most part remained the same. We had to make changes to more syscalls than we had expected in order to make them compatible.
	- By using `struct FD_PTR` to point to either a directory or file, all the syscalls that involved using a `file` had to changed to use `struct FD_PTR` instead. Many method and variable names had to be changed for proper coding style/practice.
		- We needed extra methods since instead of names, we were given a path. Thus, we had to include a `resolve_path` method and helper methods.
		- Many functions that were used in syscall had to be changed in order to be able to use a `struct FD_PTR`.
	- `remove` was a lot more complicated than we had expected due to the additional accessibility constraints wtih removing directories.

### Work Distribution

* Johnny: Design Doc, Subdirectories, Debugging, Student Testing reports
* Will: Design Doc, Extensible Files, Debugging,
* Edgar: Design Doc, Buffer Cache, Debuggig
* Angel: N/A

### Reflection

This project was probably one of the most challenging. We should have spent more time on our design doc because we ran into a lot of issues that we could have avoided had we spent more time planning. Time management was something we could have improved on. Debugging a huge pain since we did not know exactly which parts were causing crashes.

Working collaboratively would have been helpful since parts from previous projects were used and some members knew some parts better than others.

A big issue that was fustrating was that many of our bugs were in parts of the projects that we did not write.

Overall, all the members who contributed in the project put in equal amount of work.

## Student Testing Report

### Test 1: my-test-1.c

#### Description
Test your buffer cache’s effectiveness by measuring its cache hit rate.

#### Overview
- Create a file named "a" and write some bits to it. Close the file.
- Reset the buffer cache.
	- Created a `reset_buffer` syscall in order reset the cache.
- Open the "a" for the the first sequential read and then close it.
- Get the number of hits and misses and calculate the cold hit rate.
	- Needed an additional syscall `get_stats` in order to get the number of hits and misses.
- Re-open "a" for second sequtnial read and then close it.
- Get the number of hits and misses and calculate the new cache hit rate.
- If the cold hit rate was lower than the new hit rate, then the cache improved.
- Close and remove "a".

#### Output

###### my-test-1.output
```
Copying tests/filesys/extended/my-test-1 to scratch partition...
Copying tests/filesys/extended/tar to scratch partition...
qemu -hda /tmp/_9pUh1mgLT.dsk -hdb tmp.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading............
Kernel command line: -q -f extract run my-test-1
Pintos booting with 4,088 kB RAM...
382 pages available in kernel pool.
382 pages available in user pool.
Calibrating timer...  519,372,800 loops/s.
hda: 1,008 sectors (504 kB), model "QM00001", serial "QEMU HARDDISK"
hda1: 203 sectors (101 kB), Pintos OS kernel (20)
hda2: 238 sectors (119 kB), Pintos scratch (22)
hdb: 5,040 sectors (2 MB), model "QM00002", serial "QEMU HARDDISK"
hdb1: 4,096 sectors (2 MB), Pintos file system (21)
filesys: using hdb1
scratch: using hda2
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'my-test-1' into the file system...
Putting 'tar' into the file system...
Erasing ustar archive...
Executing 'my-test-1':
(my-test-1) begin
(my-test-1) create "a"
(my-test-1) open "a"
(my-test-1) close "a"
(my-test-1) reset-buffer
(my-test-1) open "a"
(my-test-1) read "a"
(my-test-1) close "a"
(my-test-1) open "a"
(my-test-1) read "a"
(my-test-1) close "a"
(my-test-1) Cache hit rate improved.
(my-test-1) end
my-test-1: exit(0)
Execution of 'my-test-1' complete.
Timer: 69 ticks
Thread: 30 idle ticks, 34 kernel ticks, 5 user ticks
hdb1 (filesys): 1451 reads, 647 writes
hda2 (scratch): 237 reads, 2 writes
Console: 1253 characters output
Keyboard: 0 keys pressed
Exception: 0 page faults
Powering off...

```

###### my-test-1.result
```
PASS
```

#### Potential Kernel Bugs
- If your kernel has no way of accessing and changing cache hit rate statistics, then we would not be able to see if cache is effective or not, and/or the rate would be wrong. 
	- This was a problem we ran into and needed to create additional methods, variables and syscalls in order to reset the cache and get cache hit info.
- If resetting the buffer cache is not an atomic operation, the kernel may crash or the hit rates could be inaccurate.
- If the cache uses a write-back policy, the kernel is susceptible to losing cache info should the kernel crash.

### Test 2: my-test-2.c

#### Description
Test your buffer cache’s ability to write full blocks to disk without reading them first. 

#### Overview
- Create and fill "a" with random bytes from the buffer.
- Get the initial `read_cnt` and `write_cnt`.
	- We re-used our `get_stats` syscall to get the cache statistics.
- Write 100 kB to "a" by writing 200 blocks.
- Get new `read_cnt` and `write_cnt`.
- Calculate the number of block_read and block_write made in the 200 writes.
	- There should be 0 block_reads and 200 block_writes
- Close and remove "a".

#### Output

###### my-test-2.output
```
Copying tests/filesys/extended/my-test-2 to scratch partition...
Copying tests/filesys/extended/tar to scratch partition...
qemu -hda /tmp/oElstNY9zJ.dsk -hdb tmp.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading............
Kernel command line: -q -f extract run my-test-2
Pintos booting with 4,088 kB RAM...
382 pages available in kernel pool.
382 pages available in user pool.
Calibrating timer...  434,176,000 loops/s.
hda: 1,008 sectors (504 kB), model "QM00001", serial "QEMU HARDDISK"
hda1: 203 sectors (101 kB), Pintos OS kernel (20)
hda2: 236 sectors (118 kB), Pintos scratch (22)
hdb: 5,040 sectors (2 MB), model "QM00002", serial "QEMU HARDDISK"
hdb1: 4,096 sectors (2 MB), Pintos file system (21)
filesys: using hdb1
scratch: using hda2
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'my-test-2' into the file system...
Putting 'tar' into the file system...
Erasing ustar archive...
Executing 'my-test-2':
(my-test-2) begin
(my-test-2) create "a"
(my-test-2) open "a"
(my-test-2) 100 kB written to "a"
(my-test-2) 404 block_read calls in 200 writes
(my-test-2) 404 block_write calls in 200 writes
(my-test-2) close "a"
(my-test-2) end
my-test-2: exit(0)
Execution of 'my-test-2' complete.
Timer: 74 ticks
Thread: 30 idle ticks, 37 kernel ticks, 7 user ticks
hdb1 (filesys): 2214 reads, 1137 writes
hda2 (scratch): 235 reads, 2 writes
Console: 1193 characters output
Keyboard: 0 keys pressed
Exception: 0 page faults
Powering off...

```

###### my-test-2.result
```
FAIL
Test output failed to match any acceptable form.

Acceptable output:
  (my-test-2) begin
  (my-test-2) create "a"
  (my-test-2) open "a"
  (my-test-2) 100 kB written to "a"
  (my-test-2) 0 block_read calls in 200 writes
  (my-test-2) 200 block_write calls in 200 writes
  (my-test-2) close "a"
  (my-test-2) end
Differences in `diff -u' format:
  (my-test-2) begin
  (my-test-2) create "a"
  (my-test-2) open "a"
  (my-test-2) 100 kB written to "a"
- (my-test-2) 0 block_read calls in 200 writes
- (my-test-2) 200 block_write calls in 200 writes
+ (my-test-2) 404 block_read calls in 200 writes
+ (my-test-2) 404 block_write calls in 200 writes
  (my-test-2) close "a"
  (my-test-2) end

(Process exit codes are excluded for matching purposes.)
```

#### Potential Kernel Bugs
- If your kernel has no way of accessing and changing cache hit rate statistics, then we would not be able to see if cache is effective or not, and/or the rate would be wrong. 
	- This was a problem we ran into and needed to create additional methods, variables and syscalls in order to reset the cache and get cache hit info.
- If block_reads were used when writing to the disk, we would have the same amount block_reads as block_writes.
- If the kernel performed a function that changed read_cnt or write_cnt before we were able to get their statistics, then the number of calls would not be accurate. 

### Experience writing tests for Pintos
For the most part, I mainly used code given to us from the grow-two-files for creating, reading, and writing to a file. A challenge that we had was finding a way to reset the buffer cache and to get statistics on the cache's performance. Initially, we wanted to avoid creating new syscalls. We tried placing them into the practice syscall but that led to other problems. In the end, we ended up creating new syscalls.

Some improvement may be in the info displayed if the test case failed. When there are multi-line differences, it was difficult to debug especially when the difference is everything being 1 position off.

We also ran into a lot of cache issues which might explain the problem we ran into for my-test-2.