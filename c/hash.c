
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "skein/SHA3api_ref.h"

const size_t LEN = 40;

const char *goal =
    "5b4da95f5fa08280fc9879df44f418c8f9f12ba424b7757de02bbdfbae0d4c4fdf9317c80c"
    "c5fe04c6429073466cf29706b8c25999ddd2f6540d4475cc977b87f4757be023f19b8f4035"
    "d7722886b78869826de916a79cf9c94cc79cd4347d24b567aa3e2390a573a373a48a5e6766"
    "40c79cc70197e1c5e7f902fb53ca1858b6";

unsigned char goalbits[128];

unsigned hex2dec(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  return 10 + (c - 'a');
}

void init_goalbits() {
  assert(strlen(goal) == 256);
  printf("Goal: ");
  for (unsigned i = 0; i < 128; ++i) {
    unsigned char b1 = hex2dec(goal[2 * i]);
    unsigned char b2 = hex2dec(goal[2 * i + 1]);
    goalbits[i] = b1 << 4 | b2;
    printf("%02x", goalbits[i]);
  }
  printf("\n");
}

void gen_rand(char *str) {
  static const char charset[] = "abcdefghijklmnopqrstuvwxyz"
                                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "0123456789";
  // TODO: Much faster (fewer calls to rand()!)
  for (unsigned i = 0; i < LEN; ++i)
    str[i] = charset[rand() % (sizeof(charset) - 1)];
}

static inline unsigned distance(unsigned x, unsigned y) {
  return __builtin_popcount(x ^ y);
}

static inline int hamming_dist(unsigned char *s1, unsigned char *s2,
                               size_t len) {
  unsigned steps = len / sizeof(unsigned);
  unsigned dist = 0;

  unsigned *p1 = (unsigned *)s1;
  unsigned *p2 = (unsigned *)s2;
  for (unsigned i = 0; i < len; ++i) {
    dist += distance(s1[i], s2[i]);
  }
  return dist;
}

volatile int global_best = 100000000;

void *search(void *unused) {
  char str[LEN + 1];
  str[LEN] = 0;

  char hash[128];
  int best = global_best;
  while (1) {
    gen_rand(str);

    Hash(1024, str, LEN * 8, hash);

    int d = hamming_dist(hash, goalbits, 128);
    if (d < best) {
      best = d;

      if (d < global_best) {
        global_best = d;
        printf("%d - '%s'\n", d, str);
      }
    }
  }
}

int main(int argc, char ** argv) {
  assert(argc > 1 && "Single argument: number of threads");
  const int NUM_THREADS = atoi(argv[1]);
  assert(NUM_THREADS > 0);
  printf("Using %d threads...\n", NUM_THREADS);

  srand(time(NULL));

  init_goalbits();

  pthread_t threads[NUM_THREADS];
  for (int i = 0; i < NUM_THREADS; ++i)
    pthread_create(&threads[i], NULL, search, NULL);

  for (int i = 0; i < NUM_THREADS; ++i)
    pthread_join(threads[i], NULL);

  return 0;
}
