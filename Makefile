
CC=gcc
CFLAGS  = -std=c99 -O3 -fno-strict-aliasing
CFLAGS += -Wall -Werror

OS := $(shell uname -s)
ifneq ($(OS),Darwin)
	CFLAGS += -march=native -mtune=native
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

hash: hash.c skein/skein.c skein/SHA3api_ref.c skein/skein_block.c | $(BSD)/.built
	$(CC) $(CFLAGS) $^ -o $@ -lpthread -lbsd -I$(BSD)/include -L$(BSD)/lib -Wl,-rpath,$(BSD)/lib

clean:
	-rm hash
	-rm -rf $(BSD) bsd-build

