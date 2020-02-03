#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main(int agrc, char** argv) {
	unsigned int i = 10;
	printf("%s%d%s\n", "Going to sleep for ", i, " seconds... Zzz..");
	sleep(i);
	printf("\nIM AWAKE! Goodbye!\n");
	return 0;
}
