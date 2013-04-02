
CC=gcc
CFLAGS  = -std=c99 -O3 -fno-strict-aliasing
CFLAGS += -Wall -Werror

OS := $(shell uname -s)
ifneq ($(OS),Darwin)
	CFLAGS += -march=native -mtune=native
endif

hash: hash.c skein/skein.c skein/SHA3api_ref.c skein/skein_block_x64.s
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

clean:
	rm hash
