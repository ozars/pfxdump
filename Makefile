LIBS=-lzidx -lz -lstreamlike -lparsebgp

all:
	gcc -g -O0 -o pfxdump ${LIBS} main.c find_prefix.c
debug:
	gcc -g -O0 -fsanitize=address -fno-omit-frame-pointer -o pfxdump ${LIBS} -lasan main.c find_prefix.c
