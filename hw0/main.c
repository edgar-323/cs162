#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>

int main() {
    /*
    Modify main.c so that it prints out the maximum STACK_SIZE,
    the maximum number of PROCESSES,
    and maximum number of FILE_DESCRIPTORS
    */

    struct rlimit lim;
    long long max_stack, max_threads, max_fds;
    /*
    RLIMIT_FSIZE
      The  maximum  size  of  files  that  the  process may create.
    RLIMIT_NOFILE
      Specifies  a  value  one  greater than the maximum file descriptor number
      that can be opened by this process.
    RLIMIT_NPROC
      The maximum number of processes (or, more precisely on Linux, threads)
      that can be created for the real user ID of the calling process.
    RLIMIT_STACK
      The  maximum  size  of  the  process  stack, in bytes.
    */

    //int my_pid = getpid();
    if (getrlimit(RLIMIT_STACK, &lim) < 0) {
      fprintf(stderr, "%s\n", "ERROR in getrlimit(RLIMIT_STACK)");
      exit(EXIT_FAILURE);
    }
    max_stack = (long long) lim.rlim_cur;

    if (getrlimit(RLIMIT_NPROC, &lim) < 0) {
      fprintf(stderr, "%s\n", "ERROR in getrlimit(RLIMIT_NPROC)");
      exit(EXIT_FAILURE);
    }
    max_threads = (long long) lim.rlim_cur;

    if (getrlimit(RLIMIT_NOFILE, &lim) < 0) {
      fprintf(stderr, "%s\n", "ERROR in getrlimit(RLIMIT_NOFILE)");
      exit(EXIT_FAILURE);
    }
    max_fds = (long long) lim.rlim_cur;

    printf("stack size: %lld\n", max_stack);
    printf("process limit: %lld\n", max_threads);
    printf("max file descriptors: %lld\n", max_fds);
    return 0;
}

