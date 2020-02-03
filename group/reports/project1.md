Final Report for Project 1: Threads
===================================

We made the following changes:

- For task 1, we decided not to use a `sleeper_thread` struct when implementing the list of sleeping threads. Instead, we used moved our `wakeup` variable to the thread struct and implemented our our list of sleeper threads using a list of threads. The reason for this change was that we were leaking memory whenever we removed structs from the list of sleep threads since we couldn't call `free()` inside the interrupt handler.

- For task 2, there was a couple of design changes that we're made.
	- To begin, we changed the type of `b_locker` from a thread pointer to a lock pointer. The reasoning behind this was that recursive priority donation needed to know which lock to donate to (if necessary) and that corresponding lock will already have a pointer to the appropriate thread.
	- Furthermore, we realized that there were several places where a new thread was introduced (woken up, created, etc) but we weren't checking if these threads had a higher priority. Thus, when creating a new thread or when waking up a sleeping thread, we yield the current thread if the new thread had a higher priority. In the case of `lock_release`, we ensure to remove all donations corresponding to the lock we are releasing before attempting to wake up a thread in order to ensure correctness.
	- Finally, various comparators were implemented for comparing different types of objects (e.g., donation objects, thread objects, etc).

- For task 3, there were some issues regarding types. Specifically, even though we were planning on using `fixed_point_t` for the calcuations, we thought we could store the final answers as ints. However, we instead made the global variables `fixed_point_t`'s and just used the `get` functions to transform them into ints.


Division of labor:

Will - design doc and MLFQS + debug

Angel - Priority scheduler / donation

Edgar - Priority scheduler / donation + debug

Johnny - Sleep

Overall everyone was fine with division of labor.

