
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "skein/SHA3api_ref.h"

const size_t LEN = 40;
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

void *search(void *unused) {
  char str[LEN + 1];
  str[LEN] = 0;

  char hash[128];
  int best = global_best;
  time_t start = time(NULL);
  size_t count = 0;
  char counting = 1;
  while (1) {
    // Generate random string, minus last 3 characters
    gen_rand(str, LEN - 3);

    // Which we enumerate through here:
    for (int i = 0; i < charset_size; ++i) {
      str[LEN - 3] = charset[i];
      for (int j = 0; j < charset_size; ++j) {
        str[LEN - 2] = charset[j];
        for (int k = 0; k < charset_size; ++k) {
          str[LEN - 1] = charset[k];
          Hash(1024, str, LEN * 8, hash);

          int d = hamming_dist(hash, goalbits, 128);
          if (d < best) {
            best = d;

            lock();
            if (d < global_best) {
              global_best = d;
              printf("%d - '%s'\n", d, str);
	      char alert[2000], uiuc[2000];
	      sprintf(alert,"curl -G -s -d 'value=%d&hashable=%s' http://ec2-54-244-215-193.us-west-2.compute.amazonaws.com", d, str);
	      system(alert);
	      sprintf(uiuc,"curl -s -d 'hashable=%s' http://almamater.xkcd.com/?edu=uiuc.edu > /dev/null", str);
	      system(uiuc);
            }
            unlock();
          }

        }
      }
    }

    if (!counting) continue;

    const size_t iters = 30;
    if (++count == iters) {
      counting = 0;

      time_t end = time(NULL);
      int elapsed = end - global_start;
      const uint64_t full_count =
          iters * (charset_size * charset_size * charset_size);

      lock();
      global_count += full_count;
      assert(global_count >= full_count);
      if (++global_done == NUM_THREADS) {
        printf("\n*** Total throughput ~= %f hash/S\n\n",
               ((double)(global_count)) / elapsed);
      }
      unlock();
    }
  }
}

int main(int argc, char ** argv) {
  assert(argc > 1 && "Single argument: number of threads");
  NUM_THREADS = atoi(argv[1]);
  assert(NUM_THREADS > 0);
  printf("Using %d threads...\n", NUM_THREADS);

  srand(time(NULL));

  init_goalbits();

  global_start = time(NULL);

  pthread_t threads[NUM_THREADS];
  for (int i = 0; i < NUM_THREADS; ++i)
    pthread_create(&threads[i], NULL, search, NULL);

  for (int i = 0; i < NUM_THREADS; ++i)
    pthread_join(threads[i], NULL);

  return 0;
}
