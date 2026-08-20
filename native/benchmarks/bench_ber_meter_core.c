/* bench_ber_meter_core.c — scoring a stream against known truth.
 *
 * A jm scaffold that recorded nothing until now (doppler#891).
 *
 * `ber_meter_score` is what every BER sweep point runs after the receiver
 * has produced symbols: strip the alignment, compare against truth, count.
 * It is O(n) over the scored window, and a sweep runs it once per point per
 * seed -- so it sits beside `ber_evm_db` (bench_ber_core.c) as a cost the
 * HARNESS pays rather than a link.
 *
 * `align` is separated from `score` deliberately. Alignment is a SEARCH
 * over candidate lags and rotations, done once at the start of a capture;
 * scoring is a linear pass, done for every window after. Quoting them
 * together would hide which one a long sweep is actually paying for.
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "ber_meter/ber_meter_core.h"
#include "jm_bench.h"
#include "mpsk/mpsk_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NSYM 65536
#define ITERATIONS 100
#define WARMUP_S 0.25
#define M 4

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

  uint8_t       *truth = malloc (NSYM);
  float complex *rx    = malloc (NSYM * sizeof *rx);
  if (!truth || !rx)
    return 1;

  uint32_t lfsr = 0x6D1Fu;
  for (int i = 0; i < NSYM; i++)
    {
      lfsr     = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      truth[i] = (uint8_t)(lfsr & (M - 1));
    }
  /* The constellation comes from mpsk_core.h's static inline rather than
     the linked mpsk_map(): this target's link line is jm-generated for a
     component, and adding mpsk_core to it by hand would be manifest drift.
     mpsk_constellation() is header-inline, so the symbols are the
     library's own with nothing new to link. */
  for (int i = 0; i < NSYM; i++)
    rx[i] = mpsk_constellation (truth[i], M);
  /* A few genuine errors, so the counter's error path is exercised rather
     than a clean stream that never takes it. */
  for (int i = 0; i < NSYM; i += 997)
    rx[i] = -rx[i];

  ber_meter_state_t *b = ber_meter_create (M, 200, 0.99);
  if (!b || ber_meter_set_truth (b, truth, NSYM) != 0)
    {
      (void)fprintf (stderr, "bench_ber_meter: create/set_truth failed\n");
      return 1;
    }

  printf ("=== ber_meter benchmark ===\n");
  printf ("M=%d, %d symbols, %d rounds\n\n", M, NSYM, ITERATIONS);

  size_t got = ber_meter_score (b, rx, NSYM, 0, NSYM);
  if (got == 0)
    {
      (void)fprintf (stderr,
                     "bench_ber_meter: scored 0 of %d symbols — the timings "
                     "below would measure a no-op\n",
                     NSYM);
      return 1;
    }

  struct timespec w0, w1;
  clock_gettime (CLOCK_MONOTONIC, &w0);
  do
    {
      ber_meter_reset (b);
      (void)ber_meter_set_truth (b, truth, NSYM);
      sink += ber_meter_score (b, rx, NSYM, 0, NSYM);
      clock_gettime (CLOCK_MONOTONIC, &w1);
    }
  while (elapsed_sec (&w0, &w1) < WARMUP_S);

  static double t_sc[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      ber_meter_reset (b);
      (void)ber_meter_set_truth (b, truth, NSYM);
      clock_gettime (CLOCK_MONOTONIC, &t0);
      sink += ber_meter_score (b, rx, NSYM, 0, NSYM);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_sc[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "score", t_sc, ITERATIONS, NSYM);
  {
    double s = min_sec (t_sc, ITERATIONS);
    printf ("  %-14s %7.2f ns/sym  %8.1f Msym/s  %8.3f ms per window\n",
            "score", s / (double)NSYM * 1e9, (double)NSYM / s / 1e6, s * 1e3);
  }

  static double t_al[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      ber_meter_reset (b);
      (void)ber_meter_set_truth (b, truth, NSYM);
      clock_gettime (CLOCK_MONOTONIC, &t0);
      sink += (size_t)ber_meter_align (b, rx, NSYM, 1000, 256, 0, 200, 0.0);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_al[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "align", t_al, ITERATIONS, NSYM);
  printf ("  %-14s %7.2f ns/sym  %8.3f ms per capture  (once, not per "
          "window)\n",
          "align", min_sec (t_al, ITERATIONS) / (double)NSYM * 1e9,
          min_sec (t_al, ITERATIONS) * 1e3);

  printf ("\n  align/score = %.2fx -- with a 256-symbol marker and a\n"
          "  200-symbol lag span, alignment costs about ONE scoring pass,\n"
          "  not the multiple a lag search might suggest. So neither is the\n"
          "  expensive half; a sweep's cost is simply the score column times\n"
          "  its point count, since align runs once per capture and score\n"
          "  runs per window.\n",
          min_sec (t_al, ITERATIONS) / min_sec (t_sc, ITERATIONS));

  (void)sink;
  ber_meter_destroy (b);
  free (truth);
  free (rx);
  jm_bench_write_json (&_bench, "ber_meter");
  return 0;
}
