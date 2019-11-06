CC=gcc
CFLAGS=-std=c99 -O3 -DNDEBUG
DEBUG_CFLAGS=-std=gnu99 -g -O0 -pg -fsanitize=address -fno-omit-frame-pointer

PFXDUMP_PROGRAM=pfxdump
PFXDUMP_SRC=main.c find_prefix.c
PFXDUMP_LIBS=-lzidx -lz -lstreamlike -lparsebgp

ZIDX_PROGRAM=zidx
ZIDX_SRC=zidx.c
ZIDX_LIBS=-lzidx -lz -lstreamlike

OUTPUT_DIR=bin

all:
	${CC} ${CFLAGS} -o "${OUTPUT_DIR}/${PFXDUMP_PROGRAM}" ${PFXDUMP_LIBS} ${PFXDUMP_SRC}
	${CC} ${CFLAGS} -o "${OUTPUT_DIR}/${ZIDX_PROGRAM}" ${ZIDX_LIBS} ${ZIDX_SRC}

debug:
	${CC} ${DEBUG_CFLAGS} -o "${OUTPUT_DIR}/${PFXDUMP_PROGRAM}" ${PFXDUMP_LIBS} ${PFXDUMP_SRC}
	${CC} ${DEBUG_CFLAGS} -o "${OUTPUT_DIR}/${ZIDX_PROGRAM}" ${ZIDX_LIBS} ${ZIDX_SRC}
clean:
	rm -f "${OUTPUT_DIR}/${PFXDUMP_PROGRAM}" "${OUTPUT_DIR}/${ZIDX_PROGRAM}"

.PHONY: all debug clean
