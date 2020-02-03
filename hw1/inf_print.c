#include <stdio.h>
#include <stdlib.h>

#include <signal.h> /*for signal() and raise()*/

void f(int sig) {
  printf("%s\n", "GOODBYE!");
  exit(0);
}

int main(int argc, char** argv) {
  signal(SIGINT, f);

  while (1) {
    printf("%s\n", "PRINT FOREVER");
  }

  return 0;
}
