CC=gcc
CFLAGS= -Wall -Wextra -Werror -Wpedantic -D_GNU_SOURCE -O3 -lpthread

fwatch: fwatch.c

.PHONY:
clean:
	rm fwatch
