#include "dp/buffer.h"
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define SAMPLES_PER_PASS (1ULL << 28) // 268M samples
#define BATCH 4096

// Generic producer for any doppler type
#define DEFINE_PRODUCER(name, type)                                           \
  void *producer_##name (void *arg)                                           \
  {                                                                           \
    dp_##name *ab = (dp_##name *)arg;                                         \
    type dummy[BATCH * 2] = { 0 };                                            \
    for (size_t i = 0; i < SAMPLES_PER_PASS; i += BATCH)                      \
      {                                                                       \
        while (!dp_##name##_write (ab, dummy, BATCH))                         \
          DP_SPIN_HINT ();                                                    \
      }                                                                       \
    return NULL;                                                              \
  }

// Generic consumer for any doppler type
#define DEFINE_CONSUMER(name, type)                                           \
  void *consumer_##name (void *arg)                                           \
  {                                                                           \
    dp_##name *ab = (dp_##name *)arg;                                         \
    for (size_t i = 0; i < SAMPLES_PER_PASS; i += BATCH)                      \
      {                                                                       \
        dp_##name##_wait (ab, BATCH);                                         \
        dp_##name##_consume (ab, BATCH);                                      \
      }                                                                       \
    return NULL;                                                              \
  }

DEFINE_PRODUCER (f32, float)
DEFINE_CONSUMER (f32, float)
DEFINE_PRODUCER (f64, double)
DEFINE_CONSUMER (f64, double)

double
run_bench (const char *label, void *(*prod) (void *), void *(*cons) (void *),
           void *ab, size_t sample_size)
{
  struct timespec start, end;
  pthread_t t1, t2;

  clock_gettime (CLOCK_MONOTONIC, &start);
  pthread_create (&t1, NULL, prod, ab);
  pthread_create (&t2, NULL, cons, ab);
  pthread_join (t1, NULL);
  pthread_join (t2, NULL);
  clock_gettime (CLOCK_MONOTONIC, &end);

  double sec
      = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
  double gb
      = (double)(SAMPLES_PER_PASS * sample_size * 2) / (1024 * 1024 * 1024);
  printf ("%s: %.2f GB/s (%.2f MSPS)\n", label, gb / sec,
          (SAMPLES_PER_PASS / sec) / 1e6);
  return gb / sec;
}

int
main ()
{
  dp_f32 *ab32 = dp_f32_create (1 << 20);
  dp_f64 *ab64 = dp_f64_create (1 << 20);

  run_bench ("Complex f32 (8 bytes/sample) ", producer_f32, consumer_f32, ab32,
             sizeof (float));
  run_bench ("Complex f64 (16 bytes/sample)", producer_f64, consumer_f64, ab64,
             sizeof (double));

  return 0;
}
