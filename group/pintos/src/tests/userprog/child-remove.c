/* Child process run by read-missing test.

   Removes "sample.txt" file. Two results are
   allowed: either the system call should return without taking
   any action, or the kernel should terminate the process with a
   -1 exit code. */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include "tests/lib.h"

const char *test_name = "child-remove";

int
main (int argc UNUSED, char *argv[] UNUSED)
{
  msg ("begin");
  remove ("sample.txt");
  msg ("end");

  return 0;
}
