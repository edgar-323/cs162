CC=g++

all: wc map main

wc: wc.c
	$(CC) -g wc.c -o wc

map: map.c
	$(CC) -g map.c -o map

main: main.c
	$(CC) -g main.c -o main

.PHONY: clean

clean:
	rm wc map main

