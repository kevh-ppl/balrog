LLIBUDEV := -ludev
GCCFLAGS := $(LLIBUDEV) -Wall -Wextra -Werror -pedantic -std=c99
CC := gcc

all: udev.c
	$(CC) udev.c -o balrog $(GCCFLAGS)

clean:
	rm -f balrog