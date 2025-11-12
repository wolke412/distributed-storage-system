all:
	gcc  		\
		-Wall -Wextra -O2 \
		-Ilib \
		-Iinclude \
		lib/*.c lib/**/*.c \
		main.c -o main

clean:
	rm -f main lib/*.o

