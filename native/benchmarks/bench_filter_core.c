/* bench_filter_core.c -- design_lowpass, the module's one free function.
 *
 * `design_lowpass` is auto-sized: the caller gives band edges and a
 * stopband attenuation, and `kaiser_num_taps` decides how many taps that
 * costs. So the caller never picks the length, and the cost is set by the
 * transition width -- which is the one thing this benchmark has to make
 * visible, because it is not visible in the signature.
 *
 * Three designs at the same 60 dB attenuation and the same passband edge,
 * differing only in how tight the transition is. A tenth of the band
 * costs a handful of taps; a hundredth costs an order of magnitude more,
 * and every one of them is a sinc, a Kaiser window value and a multiply.
 * The `ns/tap` column is what says whether the design is linear in its
 * output or worse.
 *
 * This is a construction-time call -- once per FIR, not once per sample.
 * The row exists so a caller reconfiguring a filter per frame can find
 * out what that costs before doing it, which is the question the number
 * in `bench_fir_core.c` cannot answer.
 */
#include "dp_bench.h"
#include "filter/filter_core.h"
#include "resample/resample_core.h"
#include <stdio.h>
#include <stdlib.h>

#define ITERATIONS 200
#define ATTEN_DB 60.0
#define FPASS 0.20
#define N_CFG 3

/* Nyquist-normalised stopband edges: 1/5, 1/33 and 1/133 of the band. */
static const double fstop[N_CFG] = { 0.40, 0.26, 0.215 };

/* The manifest's own out_size expression for this function -- taps are
   sized by the design, so the benchmark must not guess a length. */
static size_t
taps_for (double fp, double fs)
{
  return (size_t)(kaiser_num_taps (1, ATTEN_DB, fp / 2.0, fs / 2.0) | 1);
}

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  static double   t[N_CFG][ITERATIONS];
  size_t          n[N_CFG], n_max = 0;
  float          *out;
  char            name[64];

  for (int c = 0; c < N_CFG; c++)
    {
      n[c] = taps_for (FPASS, fstop[c]);
      if (n[c] > n_max)
        n_max = n[c];
    }

  out = malloc (n_max * sizeof *out);
  if (!out)
    return 1;

  printf ("=== filter (Kaiser-windowed-sinc lowpass design) ===\n");
  printf ("atten = %.0f dB, fpass = %.2f, %d rounds\n\n", ATTEN_DB, FPASS,
          ITERATIONS);

  DP_BENCH_SETTLE (design_lowpass (FPASS, fstop[0], ATTEN_DB, out));

  /* Rounds outside, designs inside: ns/tap is compared across the three,
     so a thermal step must not land on one transition width alone. */
  for (int r = 0; r < ITERATIONS; r++)
    for (int c = 0; c < N_CFG; c++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        design_lowpass (FPASS, fstop[c], ATTEN_DB, out);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t[c][r] = dp_bench_elapsed (&t0, &t1);
      }

  for (int c = 0; c < N_CFG; c++)
    {
      (void)snprintf (name, sizeof name, "design_lowpass[fstop=%.3f,taps=%zu]",
                      fstop[c], n[c]);
      dp_bench_record (&_bench, name, t[c], ITERATIONS, n[c], "tap");
    }

  printf ("\n  %zu taps to %zu is %.1fx the work for a transition %.1fx\n"
          "  tighter, at identical attenuation. The caller chose neither\n"
          "  length -- kaiser_num_taps did, from the band edges -- so this\n"
          "  is the cost of the SPECIFICATION, not of a tap count.\n",
          n[0], n[N_CFG - 1],
          dp_bench_min (t[N_CFG - 1], ITERATIONS)
              / dp_bench_min (t[0], ITERATIONS),
          (fstop[0] - FPASS) / (fstop[N_CFG - 1] - FPASS));

  free (out);
  jm_bench_write_json (&_bench, "filter");
  return 0;
}
