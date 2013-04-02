
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

typedef struct
{
  uint_t I,J;                         /* RC4 vars */
  u08b_t state[256];
} prng_state_t;

void randBytes(prng_state_t *prng, void *dst, uint_t byteCnt)
{
  u08b_t a,b;
  u08b_t *d = (u08b_t *) dst;

  for (;byteCnt;byteCnt--,d++)        /* run RC4  */
  {
    prng->I  = (prng->I+1) & 0xFF;
    a        =  prng->state[prng->I];
    prng->J  = (prng->J+a) & 0xFF;
    b        =  prng->state[prng->J];
    prng->state[prng->I] = b;
    prng->state[prng->J] = a;
    *d       =  charset[prng->state[(a+b) & 0xFF] % charset_size];
  }
}

/* init the (RC4-based) prng */
void Rand_Init(prng_state_t *prng, u64b_t seed)
{
  uint_t i,j;
  u08b_t tmp[512];

  /* init the "key" in an endian-independent fashion */
  for (i=0;i<8;i++)
    tmp[i] = (u08b_t) (seed >> (8*i));

  /* initialize the permutation */
  for (i=0;i<256;i++)
    prng->state[i]=(u08b_t) i;

  /* now run the RC4 key schedule */
  for (i=j=0;i<256;i++)
  {
    j = (j + prng->state[i] + tmp[i%8]) & 0xFF;
    tmp[256]      = prng->state[i];
    prng->state[i] = prng->state[j];
    prng->state[j] = tmp[256];
  }
  prng->I = prng->J = 0;  /* init I,J variables for RC4 */

  /* discard initial keystream before returning */
  randBytes(prng,tmp,sizeof(tmp));
}

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

int global_best = 100000000;
uint64_t global_count = 0;
int global_done = 0;

time_t global_start;

void check(hashState hs, char hash[1024], int*best, char *str) {
  // Output hash into buffer
  Final(&hs, hash);

  // Evaluate, update local and global best
  int d = hamming_dist(hash, goalbits, 128);
  if (d < *best) {
    *best = d;

  uint32_t local_best = global_best;
    while (d < local_best) {
      if (__sync_bool_compare_and_swap(&global_best, local_best, d)) {
        printf("%d - '%s'\n", d, str);
        char alert[2000], uiuc[2000];
        sprintf(alert,"curl -G -s -d 'value=%d&hashable=%s' http://ec2-54-244-215-193.us-west-2.compute.amazonaws.com", d, str);
        system(alert);
        sprintf(uiuc,"curl -s -d 'hashable=%s' http://almamater.xkcd.com/?edu=uiuc.edu > /dev/null", str);
        system(uiuc);
      }

      local_best = global_best;
    }
  }
}

void *search(void *arg) {
  char str[4*LEN+5];
  strcpy(str, "UIUC");

  char hash[128];
  int best = global_best;
  time_t start = time(NULL);
  uint64_t count = 0;
  prng_state_t prng;
  Rand_Init(&prng, start ^ (uintptr_t)&count);
  volatile uint64_t **counter = (volatile uint64_t **)arg;
  *counter = &count;

  while (1) {
    // Generate random string
    randBytes(&prng, str+4, LEN);
    memset(str+LEN+4,0, 3*LEN);

    // Hash first UIUC+LEN chars
    hashState hs;
    Init(&hs, 1024);
    Update(&hs, str, (LEN+4)*8);

    // Check this hash, intentionally using copied hashState
    check(hs, hash, &best, str);
    ++count;

    // Iteratively add more chars to the string,
    // idea being hashing the extra character each time
    // is cheaper (even if we need to keep copying the state)
    for (unsigned i = 0; i < 3*LEN; ++i) {
      char c = str[i];
      str[LEN+4+i] = c;
      Update(&hs, &c, 8);
      check(hs, hash, &best, str);
      ++count;
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

uint64_t get_counters(volatile uint64_t **counters, int thread_count) {
  uint64_t total = 0;
  for (int i = 0; i < thread_count; ++i) {
    total += *(counters[i]);
  }

  return total;
}

int main(int argc, char ** argv) {
  assert(argc > 1 && "Single argument: number of threads");
  NUM_THREADS = atoi(argv[1]);
  assert(NUM_THREADS > 0);
  printf("Using %d threads...\n", NUM_THREADS);

  seed();

  init_goalbits();

  global_start = time(NULL);

  volatile uint64_t *counters[NUM_THREADS];
  pthread_t threads[NUM_THREADS];
  for (int i = 0; i < NUM_THREADS; ++i) {
    counters[i] = NULL;
    pthread_create(&threads[i], NULL, search, &counters[i]);
  }

  for (int i = 0; i < NUM_THREADS; ++i) {
    while (counters[i] == NULL) sleep(1);
  }

  uint64_t start_total;
  uint64_t end_total;
  while (1) {
    start_total = get_counters(counters, NUM_THREADS);
    time_t start_seconds = time(NULL);
    sleep(10);
    end_total = get_counters(counters, NUM_THREADS);
    time_t end_seconds = time(NULL);

    uint64_t total = end_total - start_total;
    time_t elapsed = end_seconds - start_seconds;
    printf("Calculating %f hashes per second\n", (double)total / elapsed);
  }

  for (int i = 0; i < NUM_THREADS; ++i)
    pthread_join(threads[i], NULL);

  return 0;
}
