CFLAGS=-std=c99 -Wall -Wextra -pedantic -O2
CXXFLAGS=-Wall -Wextra -pedantic -O2
#
#default all: cpp c
#
#run test: cpp c
#	@echo "C Version"
#	./c
#	@echo "C++ Version"
#	./cpp
#
#c.o: c.c localely.h makefile
#	${CC} ${CFLAGS} $< -c -o $@
#
#cpp.o: cpp.cpp localely.h makefile
#	${CXX} ${CXXFLAGS} $< -c -o $@
#
#
TARGET=hexy

.PHONY: all run test clean default

default all: ${TARGET}

run test: ${TARGET}
	./${TARGET} ${TARGET}.h

${TARGET}: c.c ${TARGET}.h makefile
	${CC} ${CFLAGS} $< -o $@

cpp: cpp.cpp ${TARGET}.h makefile
	${CXX} ${CXXFLAGS} $< -o $@

clean:
	git clean -dffx

%.htm: %.md
	markdown < $< > $@

