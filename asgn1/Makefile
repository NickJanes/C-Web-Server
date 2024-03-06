CC = clang
CFLAGS = -Wall -Wextra -Werror -pedantic -Wstrict-prototypes

all: memory

memory: memory.o
	$(CC) -o memory memory.o

memory.o: memory.c
	$(CC) $(CFLAGS) -c memory.c

clean:
	rm -f memory memory.o

format:
	clang-format -i -style=file *.c