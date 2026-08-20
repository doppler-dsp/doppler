/* bench_async_dsss_receiver_core.c — acquire, refine, track, per sample.
 *
 * A jm scaffold that recorded nothing until now (doppler#891), and the last
 * of the thirty. This is the composed asynchronous DSSS receiver: it
 * searches for a code phase it has not been told, refines the estimate, and
 * then tracks -- so unlike the tracking objects it changes what it is doing
 * partway through the block being measured.
 *
 * That is exactly why the stimulus is a real capture rather than noise.
 * Every stage exits early on a signal that is not there, so a benchmark
 * over noise would time the search giving up, forever, and report a
 * receiver several times faster than one that actually locks. The
 * precondition below refuses to time anything unless the receiver reaches
 * TRACKING and returns symbols.
 *
 * The rows separate the two regimes a caller cares about, because they are
 * different costs and a single number is the average of a transient and a
 * steady state:
 *
 *   cold    a fresh receiver over the whole capture: silence, acquisition,
 *           refinement, then tracking. What a burst-mode receiver pays.
 *   warm    the same receiver re-fed the tracking half after it has locked.
 *           What a continuous receiver pays per sample once up.
 *
 * Timing is MIN over rounds, not mean, after a WARMUP_S settle.
 */
#include "async_dsss_receiver/async_dsss_receiver_core.h"
#include "jm_bench.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SF 7
#define SPC 4
#define CHIP_RATE 1.0e6
#define SYM_RATE 35714.29
#define N_SYM 400
#define ITERATIONS 20
#define WARMUP_S 0.25

static const uint8_t CODE7[SF] = { 1, 1, 1, 0, 1, 0, 0 };

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int r = 1; r < n; r++)
    if (t[r] < m)
      m = t[r];
  return m;
}

/* A DSSS capture built here rather than pulled from native/tests's
   dp_dsss_capture(): a benchmark that includes a test header acquires a
   dependency on the test tree's build wiring, and this is 15 lines. */
static size_t
build_capture (float complex **out, size_t *pre_out)
{
  const size_t   tsym = (size_t)(CHIP_RATE * SPC / SYM_RATE + 0.5); /* 112 */
  const size_t   pre  = SF * SPC * 5 + 3;
  const size_t   n    = pre + N_SYM * tsym;
  float complex *x    = malloc (n * sizeof *x);
  if (!x)
    return 0;

  for (size_t i = 0; i < pre; i++)
    x[i] = 0.0f;

  uint32_t lfsr = 0xA57Bu;
  size_t   p    = pre;
  for (size_t s = 0; s < N_SYM; s++)
    {
      lfsr    = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      float a = (lfsr & 1u) ? -1.0f : 1.0f;
      for (size_t i = 0; i < tsym; i++)
        {
          size_t chip = (i / SPC) % SF;
          x[p++]      = a * (CODE7[chip] ? -1.0f : 1.0f);
        }
    }
  *out     = x;
  *pre_out = pre;
  return n;
}

static async_dsss_receiver_state_t *
make_rx (void)
{
  return async_dsss_receiver_create (CODE7, SF, CHIP_RATE, SYM_RATE, SPC, 2,
                                     70.0, 1e-2, 0.9, 500.0, 4, 8, 0, 100.0, 4,
                                     14.0, 32, 8, false, 100000, 0.0);
}

int
main (void)
{
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };
  volatile size_t sink   = 0;

  float complex *x   = NULL;
  size_t         pre = 0;
  size_t         n   = build_capture (&x, &pre);
  if (n == 0)
    return 1;

  async_dsss_receiver_state_t *probe = make_rx ();
  if (!probe)
    {
      (void)fprintf (stderr, "bench_async_dsss_receiver: create NULL\n");
      return 1;
    }
  size_t cap = async_dsss_receiver_steps_max_out (probe);
  if (cap == 0 || cap > n)
    cap = n;
  float complex *out = malloc (cap * sizeof *out);
  if (!out)
    return 1;

  /* The precondition: a receiver that never locks would time the search's
     give-up path and read fast. */
  size_t got = async_dsss_receiver_steps (probe, x, n, out, cap);
  int    trk = async_dsss_receiver_get_tracking (probe);
  if (got == 0 || trk != 1)
    {
      (void)fprintf (stderr,
                     "bench_async_dsss_receiver: emitted %zu symbols, "
                     "tracking=%d — the timings below would measure a "
                     "receiver that never acquired\n",
                     got, trk);
      return 1;
    }
  async_dsss_receiver_destroy (probe);

  printf ("=== async_dsss_receiver benchmark ===\n");
  printf ("SF %d, spc %d, %d symbols, %zu samples, %d rounds\n\n", SF, SPC,
          N_SYM, n, ITERATIONS);

  /* Cold: a fresh receiver each round, over the whole capture. */
  static double t_cold[ITERATIONS];
  {
    struct timespec w0, w1;
    clock_gettime (CLOCK_MONOTONIC, &w0);
    do
      {
        async_dsss_receiver_state_t *rx = make_rx ();
        sink += async_dsss_receiver_steps (rx, x, n, out, cap);
        async_dsss_receiver_destroy (rx);
        clock_gettime (CLOCK_MONOTONIC, &w1);
      }
    while (elapsed_sec (&w0, &w1) < WARMUP_S);

    for (int r = 0; r < ITERATIONS; r++)
      {
        async_dsss_receiver_state_t *rx = make_rx ();
        clock_gettime (CLOCK_MONOTONIC, &t0);
        sink += async_dsss_receiver_steps (rx, x, n, out, cap);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t_cold[r] = elapsed_sec (&t0, &t1);
        async_dsss_receiver_destroy (rx);
      }
    jm_bench_add (&_bench, "steps[cold]", t_cold, ITERATIONS, (int)n);
    double sec = min_sec (t_cold, ITERATIONS);
    printf ("  %-16s %8.3f ms/capture  %7.2f ns/sample  %8.3f Msym/s\n",
            "steps[cold]", sec * 1e3, sec / (double)n * 1e9,
            (double)N_SYM / sec / 1e6);
  }

  /* Warm: one receiver, already tracking, re-fed the tracking half. */
  static double t_warm[ITERATIONS];
  {
    async_dsss_receiver_state_t *rx = make_rx ();
    sink += async_dsss_receiver_steps (rx, x, n, out, cap);
    if (async_dsss_receiver_get_tracking (rx) != 1)
      {
        (void)fprintf (stderr,
                       "bench_async_dsss_receiver: warm receiver is not "
                       "tracking — the rows below would not be steady "
                       "state\n");
        return 1;
      }
    const size_t    half = n / 2;
    struct timespec w0, w1;
    clock_gettime (CLOCK_MONOTONIC, &w0);
    do
      {
        sink += async_dsss_receiver_steps (rx, x + half, n - half, out, cap);
        clock_gettime (CLOCK_MONOTONIC, &w1);
      }
    while (elapsed_sec (&w0, &w1) < WARMUP_S);

    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        sink += async_dsss_receiver_steps (rx, x + half, n - half, out, cap);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t_warm[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "steps[warm]", t_warm, ITERATIONS, (int)(n - half));
    double sec = min_sec (t_warm, ITERATIONS);
    printf ("  %-16s %8.3f ms/block    %7.2f ns/sample\n", "steps[warm]",
            sec * 1e3, sec / (double)(n - half) * 1e9);
    async_dsss_receiver_destroy (rx);
  }

  printf ("\n  cold/warm per sample = %.2fx. The difference is acquisition\n"
          "  and refinement, paid once per burst; the warm row is what a\n"
          "  continuous receiver pays for as long as it holds lock. Sizing\n"
          "  either job on the other number is off by that factor.\n",
          (min_sec (t_cold, ITERATIONS) / (double)n)
              / (min_sec (t_warm, ITERATIONS) / (double)(n - n / 2)));

  (void)sink;
  free (x);
  free (out);
  jm_bench_write_json (&_bench, "async_dsss_receiver");
  return 0;
}
