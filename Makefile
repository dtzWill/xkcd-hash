
CC=gcc
CFLAGS  = -std=c99 -O3 -fno-strict-aliasing
CFLAGS += -Wall -Werror

BITS := $(shell getconf LONG_BIT)

OS := $(shell uname -s)
ifneq ($(OS),Darwin)
	CFLAGS += -march=native -mtune=native
	ifeq ($(BITS),64)
		SKEIN_SRC=skein/skein_block_x64.s
	else
		SKEIN_SRC=skein/skein_block_xmm32.s
	endif
else
	SKEIN_SRC=skein/skein_block.c
endif
hash: hash.c skein/skein.c skein/SHA3api_ref.c $(SKEIN_SRC)
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

clean:
	rm hash
