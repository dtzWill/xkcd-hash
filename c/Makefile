
CC=gcc
CFLAGS=-std=c99 -O3 -ftree-vectorize -march=native -mtune=native

hash: hash.c skein/skein.c skein/SHA3api_ref.c skein/skein_block_x64.s
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

clean:
	rm hash
