
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

all: hash

BSD := $(PWD)/bsd-install

$(BSD)/.built:
	-rm -rf bsd-build $(BSD)
	mkdir -p bsd-build
	cd bsd-build && \
	../libbsd-0.4.2/configure --prefix=$(BSD)
	$(MAKE) -C bsd-build
	$(MAKE) -C bsd-build install
	touch $@

hash: hash.c skein/skein.c skein/SHA3api_ref.c $(SKEIN_SRC) | $(BSD)/.built
	$(CC) $(CFLAGS) $^ -o $@ -lpthread -lbsd -I$(BSD)/include -L$(BSD)/lib -Wl,-rpath,$(BSD)/lib

clean:
	-rm hash
	-rm -rf $(BSD) bsd-build
