/* Test your buffer cache’s effectiveness by measuring its cache hit rate.
First, reset the buffer cache.
Open a file and read it sequentially, to determine the cache hit rate for
a cold cache.
Then, close it, re-open it, and read it sequentially again,
to make sure that the cache hit rate improves. */

#include <random.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

#define BLOCK_SIZE 512
#define BLOCK_CNT 64
static char buf_a[BLOCK_SIZE];

void
test_main (void)
{
  int fd_a;
  size_t retval;

  random_init (0);
  random_bytes (buf_a, sizeof buf_a);

  CHECK (create ("a", 0), "create \"a\"");

  CHECK ((fd_a = open ("a")) > 1, "open \"a\"");

  int i;
  for (i = 0; i < BLOCK_CNT; i++) {
  	retval = write (fd_a, buf_a, BLOCK_SIZE);
  	if (retval != BLOCK_SIZE)
        fail ("write %zu bytes in \"%s\" returned %zu",
              BLOCK_SIZE, "a", retval);
  }

  msg("close \"a\"");
  close (fd_a);

  msg("reset buffer");
  reset_buffer ();

  CHECK ((fd_a = open ("a")) > 1, "open \"a\"");

  msg("read \"a\"");
  for (i = 0; i < BLOCK_CNT; i++) {
  	retval = write (fd_a, buf_a, BLOCK_SIZE);
  	if (retval != BLOCK_SIZE)
        fail ("write %zu bytes in \"%s\" returned %zu",
              BLOCK_SIZE, "a", retval);
  }

  msg("close \"a\"");
  close (fd_a);

  int cold_hit = get_stats (0);
  int cold_miss = get_stats (1);
  int cold_rate = 100 * cold_hit/(cold_hit + cold_miss);

  CHECK ((fd_a = open ("a")) > 1, "open \"a\"");

  msg("read \"a\"");
  for (i = 0; i < BLOCK_CNT; i++) {
  	retval = write (fd_a, buf_a, BLOCK_SIZE);
  	if (retval != BLOCK_SIZE)
        fail ("write %zu bytes in \"%s\" returned %zu",
              BLOCK_SIZE, "a", retval);
  }

  msg("close \"a\"");
  close (fd_a);

  remove ("a");

  int new_hit = get_stats (0);
  int new_miss = get_stats (1);
  int new_rate = 100 * (new_hit - cold_hit)/((new_hit + new_miss) - (cold_hit + cold_miss));

  if (cold_rate < new_rate)
  	msg("Cache hit rate improved.");
}
