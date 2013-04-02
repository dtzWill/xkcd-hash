
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>

#include "skein/SHA3api_ref.h"

const size_t LEN = 120;
int NUM_THREADS;

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

static const char charset[] = "abcdefghijklmnopqrstuvwxyz"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "0123456789"
                              "_-.*";

const uint64_t charset_size = sizeof(charset) - 1;

void gen_rand(char *str, size_t len) {
  for (size_t i = 0; i < len; ++i)
    str[i] = charset[rand() % charset_size];
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
  for (unsigned i = 0; i < steps; ++i) {
    dist += distance(p1[i], p2[i]);
  }
  return dist;
}


pthread_mutex_t global_lock;

int global_best = 100000000;
uint64_t global_count = 0;
int global_done = 0;

time_t global_start;

void lock() {
  pthread_mutex_lock(&global_lock);
}

void unlock() {
  pthread_mutex_unlock(&global_lock);
}

void check(hashState hs, char hash[1024], int*best, char *str, uint64_t *count) {
  // Output hash into buffer
  Final(&hs, hash);

  // Evaluate, update local and global best
  int d = hamming_dist(hash, goalbits, 128);
  if (d < *best) {
    *best = d;

    lock();
    if (d < global_best) {
      global_best = d;
      printf("%d - '%s'\n", d, str);
    }
    unlock();
  }

  (*count)++;
}

void *search(void *unused) {
  char str[4*LEN+5];
  strcpy(str, "UIUC");

  char hash[128];
  int best = global_best;
  time_t start = time(NULL);
  uint64_t count = 0;
  char counting = 1;
  while (1) {
    // Generate random string
    gen_rand(str+4, LEN);
    memset(str+LEN+4,0, 3*LEN);

    // Hash first UIUC+LEN chars
    hashState hs;
    Init(&hs, 1024);
    Update(&hs, str, (LEN+4)*8);

    // Check this hash, intentionally using copied hashState
    check(hs, hash, &best, str, &count);

    // Iteratively add more chars to the string,
    // idea being hashing the extra character each time
    // is cheaper (even if we need to keep copying the state)
    for (unsigned i = 0; i < 3*LEN; ++i) {
      char c = str[i];
      str[LEN+4+i] = c;
      Update(&hs, &c, 8);
      check(hs, hash, &best, str, &count);
    }

    if (!counting) continue;

    const uint64_t iters = 10000000; // 10M
    if (count > iters) {
      counting = 0;

      time_t end = time(NULL);
      int elapsed = end - global_start;

      lock();
      global_count += count;
      assert(global_count >= count);
      if (++global_done == NUM_THREADS) {
        printf("\n*** Total throughput ~= %f hash/S\n\n",
               ((double)(global_count)) / elapsed);
      }
      unlock();
    }
  }
}

void seed() {
  struct utsname name;
  if (uname(&name) == -1) {
    perror("uname");
    exit(-1);
  }
  printf("Hostname=%s\n", name.nodename);
  int seed;
  Hash(sizeof(int) * 8, name.nodename, strlen(name.nodename) * 8, (uint8_t *)&seed);
  seed += time(NULL);

  printf("seed=%d\n", seed);

  srand(seed);
}

int main(int argc, char ** argv) {
  assert(argc > 1 && "Single argument: number of threads");
  NUM_THREADS = atoi(argv[1]);
  assert(NUM_THREADS > 0);
  printf("Using %d threads...\n", NUM_THREADS);

  seed();

  init_goalbits();

  global_start = time(NULL);

  pthread_t threads[NUM_THREADS];
  for (int i = 0; i < NUM_THREADS; ++i)
    pthread_create(&threads[i], NULL, search, NULL);

  for (int i = 0; i < NUM_THREADS; ++i)
    pthread_join(threads[i], NULL);

  return 0;
}
