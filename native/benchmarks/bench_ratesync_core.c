/* bench_ratesync_core.c — symbol timing recovery, per detector.
 *
 * A jm scaffold that recorded nothing until now (doppler#891). `ratesync`
 * is the timing loop every M-PSK receiver runs, at the SAMPLE rate: a
 * matched-filter bank lookup, an interpolation, a TED, and a loop update
 * per sample. It is one of the two things that set a receiver's ceiling
 * (the other being the carrier loop), and nothing had timed it.
 *
 * The rows sweep the two knobs a caller actually chooses:
 *
 *   ted        the detector. Gardner needs the half-symbol strobe;
 *              the others differ in how many taps they touch per update
 *   sps        samples per symbol. The loop runs per sample but updates
 *              per symbol, so a higher sps spreads the update cost over
 *              more samples and should read CHEAPER per sample -- the
 *              opposite of the intuition that oversampling costs more
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "jm_bench.h"
#include "ratesync/ratesync_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 50
#define NUM_PHASES 1024
#define SPAN 8
#define M_ARMS 2

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

int
main (void)
{
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };
  volatile size_t sink   = 0;

  float complex *x = malloc (BENCH_N * sizeof *x);
  float complex *y = malloc (BENCH_N * sizeof *y);
  if (!x || !y)
    return 1;

  /* A modulated stream, not a constant: the TED's output feeds the loop,
     and a DC input would leave the NCO parked where a real one never is. */
  uint32_t lfsr = 0x77A1u;
  for (int i = 0; i < BENCH_N; i++)
    {
      lfsr    = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      float s = (lfsr & 1u) ? -1.0f : 1.0f;
      x[i]    = s * (float)cos (i * 0.013) + s * (float)sin (i * 0.013) * I;
    }

  printf ("=== ratesync benchmark ===\n");
  printf ("block = %d samples, %d phases, span %d, %d rounds\n\n", BENCH_N,
          NUM_PHASES, SPAN, ITERATIONS);

  const int         teds[2]  = { RATESYNC_TED_GARDNER, RATESYNC_TED_DTTL };
  const char *const tname[2] = { "gardner", "dttl" };
  const double      spss[2]  = { 4.0, 8.0 };
  static double     t_st[2][2][ITERATIONS];

  for (int d = 0; d < 2; d++)
    for (int p = 0; p < 2; p++)
      {
        ratesync_state_t *s
            = ratesync_create (spss[p], RATESYNC_PULSE_RRC, 0.35, SPAN, M_ARMS,
                               NUM_PHASES, 0.01, 0.707, teds[d]);
        if (!s)
          {
            (void)fprintf (stderr,
                           "bench_ratesync: create(sps=%.0f, ted=%s) NULL\n",
                           spss[p], tname[d]);
            return 1;
          }
        /* Capacity is the INPUT length, as the tests pass it -- the
           object emits at most one symbol per input sample and clamps
           itself. Sizing it from ratesync_steps_max_out() instead made
           every call return 0 and the benchmark report 3.2 THz, which is
           the bail-out path wearing a throughput number. */
        const size_t cap     = BENCH_N;
        size_t       got     = ratesync_steps (s, x, BENCH_N, y, cap);
        const size_t want_lo = (size_t)((double)BENCH_N / spss[p] * 0.5);
        if (got < want_lo)
          {
            (void)fprintf (stderr,
                           "bench_ratesync: %s/sps=%.0f emitted %zu symbols "
                           "from %d samples (expected >= %zu) — the timings "
                           "below would measure a no-op\n",
                           tname[d], spss[p], got, BENCH_N, want_lo);
            return 1;
          }
        for (int r = 0; r < ITERATIONS; r++)
          {
            clock_gettime (CLOCK_MONOTONIC, &t0);
            sink += ratesync_steps (s, x, BENCH_N, y, cap);
            clock_gettime (CLOCK_MONOTONIC, &t1);
            t_st[d][p][r] = elapsed_sec (&t0, &t1);
          }
        char name[64];
        (void)snprintf (name, sizeof name, "steps[%s,sps=%.0f]", tname[d],
                        spss[p]);
        jm_bench_add (&_bench, name, t_st[d][p], ITERATIONS, BENCH_N);
        double sec = min_sec (t_st[d][p], ITERATIONS);
        printf ("  %-24s %7.2f ns/sample  %8.1f MSa/s  %8.2f Msym/s\n", name,
                sec / (double)BENCH_N * 1e9, (double)BENCH_N / sec / 1e6,
                (double)BENCH_N / spss[p] / sec / 1e6);
        ratesync_destroy (s);
      }

  printf ("\n  Read the Msym/s column, not MSa/s, when sizing a link: the\n"
          "  loop runs per sample but delivers per symbol, so sps moves the\n"
          "  two in opposite directions. dttl/gardner at sps=4 is %.2fx.\n",
          min_sec (t_st[1][0], ITERATIONS) / min_sec (t_st[0][0], ITERATIONS));

  (void)sink;
  free (x);
  free (y);
  jm_bench_write_json (&_bench, "ratesync");
  return 0;
}
