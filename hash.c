//===-- hash.c ------------------------------------------------------------===//
//
// XKCD Hash SKein1024 brute forcing.
// (c) 2013 Will Dietz, <w@wdtz.org>
//
// Please see included README.md for usage and high-level documentation.
//
//===----------------------------------------------------------------------===//

#define _XOPEN_SOURCE 600

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#include "skein/SHA3api_ref.h"

//===-- Configuration -----------------------------------------------------===//

// Length of generated strings
const size_t LEN = 128;

// Vanity strings hardcoded at beginning/end of generated inputs
const char PREFIX_STRING[] = "UIUC.edu-";
const char SUFFIX_STRING[] = "-UIUC.edu";

// How many characters do we exhaustively search for each random prefix?
const size_t SEARCH_CHARS = 6;

// Character set to use.  Limited by what the web form seems to accept.
static const char CHARSET[] = "abcdefghijklmnopqrstuvwxyz"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "0123456789"
                              "_-.*";
const uint64_t CHARSET_SIZE = sizeof(CHARSET) - 1;

// Target hash.
char GOAL_BITS[128] = {
  0x5b, 0x4d, 0xa9, 0x5f, 0x5f, 0xa0, 0x82, 0x80, 0xfc, 0x98, 0x79, 0xdf, 0x44,
  0xf4, 0x18, 0xc8, 0xf9, 0xf1, 0x2b, 0xa4, 0x24, 0xb7, 0x75, 0x7d, 0xe0, 0x2b,
  0xbd, 0xfb, 0xae, 0x0d, 0x4c, 0x4f, 0xdf, 0x93, 0x17, 0xc8, 0x0c, 0xc5, 0xfe,
  0x04, 0xc6, 0x42, 0x90, 0x73, 0x46, 0x6c, 0xf2, 0x97, 0x06, 0xb8, 0xc2, 0x59,
  0x99, 0xdd, 0xd2, 0xf6, 0x54, 0x0d, 0x44, 0x75, 0xcc, 0x97, 0x7b, 0x87, 0xf4,
  0x75, 0x7b, 0xe0, 0x23, 0xf1, 0x9b, 0x8f, 0x40, 0x35, 0xd7, 0x72, 0x28, 0x86,
  0xb7, 0x88, 0x69, 0x82, 0x6d, 0xe9, 0x16, 0xa7, 0x9c, 0xf9, 0xc9, 0x4c, 0xc7,
  0x9c, 0xd4, 0x34, 0x7d, 0x24, 0xb5, 0x67, 0xaa, 0x3e, 0x23, 0x90, 0xa5, 0x73,
  0xa3, 0x73, 0xa4, 0x8a, 0x5e, 0x67, 0x66, 0x40, 0xc7, 0x9c, 0xc7, 0x01, 0x97,
  0xe1, 0xc5, 0xe7, 0xf9, 0x02, 0xfb, 0x53, 0xca, 0x18, 0x58, 0xb6
};


//===-- Global state ------------------------------------------------------===//

// Number of threads being used, set by main().
int num_threads;

// Lock to protect global state
pthread_mutex_t global_lock;

// What's the best score across all threads?
int global_best = INT_MAX;

// Throughput calculation variables:
// How many hashes have been computed by threads
// that reached the threshold?
uint64_t global_count = 0;

// How many threads have hit the threshold?
// Used to determine when all threads have done so.
int global_done = 0;

// When did this program start?
time_t global_start;

//===-- Utility Functions -------------------------------------------------===//

// Convenience wrappers for pthread functions.
void lock() { pthread_mutex_lock(&global_lock); }
void unlock() { pthread_mutex_unlock(&global_lock); }

// Populate 'str' with 'len' random characters from the character set.
void gen_rand(char *str, size_t len) {
  for (size_t i = 0; i < len; ++i)
    str[i] = CHARSET[random() % CHARSET_SIZE];
}

static inline unsigned distance(unsigned x, unsigned y) {
  // XXX: This may be slower than bit twiddling on some archs w/o popcnt.
  // Also, __builtin_popcountll is faster when it's a native instruction,
  // but leaving this as-is for wider compatability.
  return __builtin_popcount(x ^ y);
}

// Compute the hamming distance of two byte arrays of length 'len'
static inline int hamming_dist(char *s1, char *s2, size_t len) {
  unsigned steps = len / sizeof(unsigned);
  unsigned dist = 0;

  unsigned *p1 = (unsigned *)s1;
  unsigned *p2 = (unsigned *)s2;
  for (unsigned i = 0; i < steps; ++i) {
    dist += distance(p1[i], p2[i]);
  }
  return dist;
}

//===-- Worker Thread -----------------------------------------------------===//

void *search(void *unused) {
  // Various size constants for convenience
  const size_t prelen = strlen(PREFIX_STRING);
  const size_t sufflen = strlen(SUFFIX_STRING);
  const size_t suffstart = LEN - sufflen;

  // Buffer used to store candidate string, extra for null terminator.
  char str[LEN + 1];
  str[LEN] = 0;

  // Put in our hardcoded prefix/suffix strings
  strcpy(str, PREFIX_STRING);
  strcpy(str + suffstart, SUFFIX_STRING);

  // Track how many hashes we've tried, for throughput estimate.
  uint64_t count = 0;
  char counting = 1;

  // Thread-local best score achieved, used to avoid
  // unnecessary accesses to shared global_best in the common case.
  int best = INT_MAX;

  // Buffer for storing computed hash bytes
  char hash[128];

start:
  // Fill the middle of the string with random data
  // (not including prefix, suffix, or portion we'll exhaustively search)
  gen_rand(str + prelen, LEN - prelen - sufflen - SEARCH_CHARS);

  // Iteration index array
  // Indirection used to keep character set flexible.
  unsigned idx[SEARCH_CHARS];
  memset(idx, 0, sizeof(idx));

  // Initialize enumeration part of string to first letter in charset
  char *iterstr = str + suffstart - SEARCH_CHARS;
  memset(iterstr, CHARSET[0], SEARCH_CHARS);

  while (1) {
    // Try string in current form
    Hash(1024, (BitSequence *)str, LEN * 8, (BitSequence *)hash);

    // How'd we do?
    int d = hamming_dist(hash, GOAL_BITS, 128);

    // If this is the best we've seen, print it and update best.
    if (d < best) {
      best = d;

      lock();
      if (d < global_best) {
        global_best = d;
        printf("%d - '%s'\n", d, str);
        fflush(stdout);
      }
      unlock();
    }

    // Increment string index array, updating str as we go
    // aaaaaa
    // baaaaa
    // caaaaa
    // ...
    // abaaaa
    // bbaaaa
    // cbaaaa
    // ...
    // (etc)
    int cur = 0;
    while (++idx[cur] >= CHARSET_SIZE) {
      idx[cur] = 0;
      iterstr[cur] = CHARSET[idx[cur]];

      // Advance to next position.
      // If we've used all of our search characters,
      // time to start over with new random prefix.
      if (++cur == SEARCH_CHARS)
        goto start;
    }
    iterstr[cur] = CHARSET[idx[cur]];

    // Throughput calculation.
    // Once this thread hits a limit, increment global_done
    // and print throughput estimate if we're the last thread to do so.
    const uint64_t iters = 1 << 24; // ~16M
    if (counting && ++count == iters) {
      counting = 0;

      time_t end = time(NULL);
      int elapsed = end - global_start;

      lock();
      global_count += count;
      assert(global_count >= count && "counter overflow");
      if (++global_done == num_threads) {
        printf("\n*** Total throughput ~= %f hash/S\n\n",
               ((double)(global_count)) / elapsed);
      }
      unlock();
    }
  }
}

//===-- PRNG Seeding ------------------------------------------------------===//

// Seed random with hash of hostname combined with current time.
void seed() {
  struct utsname name;
  if (uname(&name) == -1) {
    perror("uname");
    exit(-1);
  }
  printf("Hostname=%s\n", name.nodename);

  unsigned seed;
  Hash(sizeof(unsigned) * 8, (BitSequence *)name.nodename,
       strlen(name.nodename) * 8, (BitSequence *)&seed);
  seed += time(NULL);

  printf("seed=%u\n", seed);

  static char state[256];
  initstate(seed, state, 256);
}

//===-- Main -------------------------------------------------------------===//

int main(int argc, char **argv) {
  if (argc > 1)
    num_threads = atoi(argv[1]);
  else
    num_threads = sysconf(_SC_NPROCESSORS_ONLN);

  assert(num_threads > 0);
  printf("Using %d threads...\n", num_threads);

  // Initialize global state
  seed();
  global_start = time(NULL);

  // Spawn worker threads
  pthread_t threads[num_threads];
  for (int i = 0; i < num_threads; ++i)
    pthread_create(&threads[i], NULL, search, NULL);

  // Wait for them, although they never return.
  for (int i = 0; i < num_threads; ++i)
    pthread_join(threads[i], NULL);

  return 0;
}
