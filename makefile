CFLAGS=-Wall -Wextra -pedantic -O2 -std=c99

.PHONY: all test clean

all: hex

test: hex
	./hex hex.c

clean:
	rm -fv *.exe hex
