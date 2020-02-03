/* Test your buffer cache’s ability to write full blocks to disk without reading them first. 
If you are, for example, writing 100KB (200 blocks) to a file, your buffer cache should 
perform 200 calls to block_write, but 0 calls to block_read, since exactly 200 blocks 
worth of data are being written. (Read operations on inode metadata are still acceptable.) 
As mentioned earlier, each block device keeps a read_cnt counter and a write_cnt counter. 
You can use this to verify that your buffer cache does not introduce unnecessary block reads. */

#include <random.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

#define BLOCK_SIZE 512
#define BLOCK_CNT 200
static char buf_a[BLOCK_SIZE];

void
test_main (void)
{
  int fd_a;
  int read_a;
  int write_a;

  size_t retval;

  random_init (0);
  random_bytes (buf_a, sizeof buf_a);

  CHECK (create ("a", 0), "create \"a\"");

  CHECK ((fd_a = open ("a")) > 1, "open \"a\"");

  read_a = get_stats (2);
  write_a = get_stats (3);

  msg ("100 kB written to \"a\"");
  int i;
  for (i = 0; i < BLOCK_CNT; i++) {
    retval = write (fd_a, buf_a, BLOCK_SIZE);
    if (retval != BLOCK_SIZE)
        fail ("write %zu bytes in \"%s\" returned %zu",
              BLOCK_SIZE, "a", retval);
  }

  int read_b;
  int write_b;
  read_b = get_stats (2);
  write_b = get_stats (3);

  msg("%d block_read calls in 200 writes", read_b - read_a);
  msg("%d block_write calls in 200 writes", write_b - write_a);
  msg ("close \"a\"");

  close (fd_a);
  remove("a");
}
