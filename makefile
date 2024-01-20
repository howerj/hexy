CFLAGS=-Wall -Wextra -pedantic -O2 -std=c99
TARGET=hexy

.PHONY: all test clean

all: ${TARGET}

test: ${TARGET}
	./${TARGET} ${TARGET}.c

clean:
	rm -fv *.exe ${TARGET}
