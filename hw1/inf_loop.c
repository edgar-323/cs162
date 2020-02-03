#include <signal.h>
#include <stdio.h>
#include <stdlib.h>


void my_handle(int sig) {
	printf("\nExecutable Received SIGNIT\n");
	FILE* file_ptr = fopen("log.txt", "a");
	fputs("\nEXECUTION IN PROGRESS\n", file_ptr);
	fclose(file_ptr);
	exit(EXIT_SUCCESS);
}

int main(int argc, char** argv) {
	//signal(SIGINT, my_handle);
	while (1);
	return 0;
}
