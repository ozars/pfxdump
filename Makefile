CC=gcc
LIBS=-lzidx -lz -lstreamlike -lparsebgp

PROGRAM=pfxdump
SRC=main.c find_prefix.c

all:
	${CC} -std=c99 -O3 -DNDEBUG -o ${PROGRAM} ${LIBS} ${SRC}
debug:
	${CC} -std=c99 -g -O0 -fsanitize=address -fno-omit-frame-pointer -o ${PROGRAM} ${LIBS} -lasan ${SRC}
clean:
	rm -f ${PROGRAM}
.PHONY: all debug
