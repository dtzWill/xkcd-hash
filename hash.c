
#define _XOPEN_SOURCE 600

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>

#include "skein/SHA3api_ref.h"

const size_t LEN = 128;
int NUM_THREADS; // Set by single argument to main

// Vanity strings hardcoded at beginning/end of generated inputs
const char prefix[] = "UIUC.edu-";
const char suffix[] = "-UIUC.edu";

// How many characters do we exhaustively search for each random prefix?
const size_t SEARCH_CHARS = 6;

// Characer set to use.  Limited by what the web form seems to accept.
static const char charset[] = "abcdefghijklmnopqrstuvwxyz"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "0123456789"
                              "_-.*";
const uint64_t charset_size = sizeof(charset) - 1;

const char *goal =
    "5b4da95f5fa08280fc9879df44f418c8f9f12ba424b7757de02bbdfbae0d4c4fdf9317c80c"
    "c5fe04c6429073466cf29706b8c25999ddd2f6540d4475cc977b87f4757be023f19b8f4035"
    "d7722886b78869826de916a79cf9c94cc79cd4347d24b567aa3e2390a573a373a48a5e6766"
    "40c79cc70197e1c5e7f902fb53ca1858b6";

char goalbits[128];

unsigned hex2dec(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  return 10 + (c - 'a');
}

// Convert the string representation of the goal hash
// into the corresponding byte array for comparison.
void init_goalbits() {
  assert(strlen(goal) == 256);
  printf("Goal: ");
  for (unsigned i = 0; i < 128; ++i) {
    unsigned char b1 = hex2dec(goal[2 * i]);
    unsigned char b2 = hex2dec(goal[2 * i + 1]);
    goalbits[i] = b1 << 4 | b2;
    printf("%02x", (unsigned char) goalbits[i]);
  }
  printf("\n");
}

// Populate 'str' with 'len' random characters from the character set.
void gen_rand(char *str, size_t len) {
  for (size_t i = 0; i < len; ++i)
    str[i] = charset[random() % charset_size];
}

static inline unsigned distance(unsigned x, unsigned y) {
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

// Lock to protect global state, below
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

void lock() { pthread_mutex_lock(&global_lock); }

void unlock() { pthread_mutex_unlock(&global_lock); }

void *search(void *unused) {
  // Various size constants for convenience
  const size_t prelen = strlen(prefix);
  const size_t sufflen = strlen(suffix);
  const size_t suffstart = LEN - sufflen;

  // Buffer used to store candidate string, extra for null terminator.
  char str[LEN + 1];
  str[LEN] = 0;

  // Put in our hardcoded prefix/suffix strings
  strcpy(str, prefix);
  strcpy(str + suffstart, suffix);

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
  unsigned idx[SEARCH_CHARS];
  memset(idx, 0, sizeof(idx));

  // Initialize enumeration part of string to first letter in charset
  char *iterstr = str + suffstart - SEARCH_CHARS;
  memset(iterstr, charset[0], SEARCH_CHARS);

  while (1) {
    // Try string in current form
    Hash(1024, (BitSequence *)str, LEN * 8, (BitSequence *)hash);

    // How'd we do?
    int d = hamming_dist(hash, goalbits, 128);

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
    while (++idx[cur] >= charset_size) {
      idx[cur] = 0;
      iterstr[cur] = charset[idx[cur]];
      if (++cur == SEARCH_CHARS)
        goto start;
    }
    iterstr[cur] = charset[idx[cur]];

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
      if (++global_done == NUM_THREADS) {
        printf("\n*** Total throughput ~= %f hash/S\n\n",
               ((double)(global_count)) / elapsed);
      }
      unlock();
    }
  }
}

// Seed random with hash of hostname combined with current time.
void seed() {
  struct utsname name;
  if (uname(&name) == -1) {
    perror("uname");
    exit(-1);
  }
  printf("Hostname=%s\n", name.nodename);
  int seed;
  Hash(sizeof(int) * 8, (BitSequence *)name.nodename, strlen(name.nodename) * 8,
       (BitSequence *)&seed);
  seed += time(NULL);

  printf("seed=%d\n", seed);

  static char state[256];
  initstate(seed, state, 256);
}

int main(int argc, char **argv) {
  assert(argc > 1 && "Single argument: number of threads");
  NUM_THREADS = atoi(argv[1]);
  assert(NUM_THREADS > 0);
  printf("Using %d threads...\n", NUM_THREADS);

  // Initialize global state
  seed();
  init_goalbits();
  global_start = time(NULL);

  // Spawn worker threads
  pthread_t threads[NUM_THREADS];
  for (int i = 0; i < NUM_THREADS; ++i)
    pthread_create(&threads[i], NULL, search, NULL);

  // Wait for them, although they never return.
  for (int i = 0; i < NUM_THREADS; ++i)
    pthread_join(threads[i], NULL);

  return 0;
}
