all:
	gcc -Wall -Wextra -O2 -Ilib  lib/*.c lib/**/*.c main.c -o main

clean:
	rm -f main lib/*.o

