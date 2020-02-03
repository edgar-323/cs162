/*two different threads open the the same file, 
then one of the threads removes it, 
and we ensure that the second thread can still read/write 
from the file until it closes it (or removes it) */

#include <stdio.h>
#include <syscall.h>
#include "tests/userprog/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"


void
test_main (void)
{
  char child_cmd[128];
  int handle;

  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");

  snprintf (child_cmd, sizeof child_cmd, "child-remove %s", "sample.txt");

  msg ("wait(exec()) = %d", wait (exec (child_cmd)));

  check_file_handle (handle, "sample.txt", sample, sizeof sample - 1);
}
