CFLAGS=-std=c99 -Wall -Wextra -pedantic -O2 -Wmissing-prototypes -fwrapv
CXXFLAGS=-Wall -Wextra -pedantic -O2
TARGET=hexy

.PHONY: all run test clean default

default all: ${TARGET}

run test: ${TARGET} lib${TARGET}.a
	./${TARGET} -t
	./${TARGET} ${TARGET}.h

${TARGET}: c.c ${TARGET}.h makefile
	${CC} ${CFLAGS} $< -o $@

cpp: cpp.cpp ${TARGET}.h makefile
	${CXX} ${CXXFLAGS} $< -o $@

${TARGET}.o: hexy.c hexy.h makefile
	${CC} ${CFLAGS} $< -c -o $@

lib.o: lib.c hexy.h makefile

lib${TARGET}.a: lib.o
	ar rcs $@ lib.o

clean:
	git clean -dffx

%.htm: %.md
	markdown < $< > $@

