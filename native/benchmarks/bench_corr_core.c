/* bench_corr_core.c -- what a dwell buys, and what the dump costs.
 *
 * `corr_execute` has two prices behind one signature. Every call runs a
 * forward FFT of the frame, multiplies pointwise by the stored reference
 * spectrum and adds that into a coherent accumulator. Only the
 * `dwell`-th call also runs the INVERSE transform and normalises, and
 * only that call writes an output; the rest return 0 and look, from the
 * outside, exactly the same.
 *
 * So a caller choosing `dwell` is amortising one inverse FFT over N
 * frames, and the useful figure is not the average -- it is the two
 * prices side by side, plus what the average becomes at each dwell. This
 * file measures the accumulate-only call and the dump call separately
 * (by driving the same correlator to the frame before its dump, then
 * timing one of each), and reports the per-frame average that implies
 * for several dwells.
 *
 * Deferring the inverse is only legal because the integration is
 * COHERENT -- a complex sum, so summing the cross-spectra and inverting
 * once equals inverting each and summing, by linearity. A non-coherent
 * (magnitude) integration could not do this and would pay the inverse
 * every frame; the ratio here is exactly what that choice would cost.
 *
 * Two frame lengths, because the accumulate and the dump scale
 * differently: the accumulate is one FFT plus an O(n) multiply-add, the
 * dump adds a second FFT and an O(n) scale, so the ratio between them
 * should be close to flat in n and any drift in it is the O(n) parts
 * moving against the O(n log n) ones.
 */
#include "corr/corr_core.h"
#include "dp_bench.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define ITERATIONS 100
#define N_LEN 2
#define DWELL 8

static const size_t frame_n[N_LEN] = { 1024, 16384 };

/* Dwells a caller might actually pick, for the amortisation table. */
#define N_DWELL 4
static const size_t dwells[N_DWELL] = { 1, 4, 16, 64 };

enum
{
  CFG_ACC,
  CFG_DUMP,
  N_KIND
};

static const char *kind_name[N_KIND]
    = { "execute[accumulate]", "execute[dump]" };

#define N_CFG (N_LEN * N_KIND)

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  static double   t[N_CFG][ITERATIONS];
  corr_state_t   *corr[N_LEN] = { 0 };
  const size_t    n_max       = frame_n[N_LEN - 1];
  float complex  *ref = NULL, *in = NULL, *out = NULL;
  char            name[64];

  ref = malloc (n_max * sizeof *ref);
  in  = malloc (n_max * sizeof *in);
  out = malloc (n_max * sizeof *out);
  if (!ref || !in || !out)
    return 1;

  /* A chirp reference and an offset copy as input: representative of the
     acquisition case, and never a value the transform can shortcut. */
  for (size_t i = 0; i < n_max; i++)
    {
      const double p = 1e-4 * (double)i * (double)i;
      ref[i]         = (float complex) (cos (p) + sin (p) * I);
      in[i]          = (float complex) (cos (p + 0.7) + sin (p + 0.7) * I);
    }

  for (int l = 0; l < N_LEN; l++)
    {
      corr[l] = corr_create (ref, frame_n[l], DWELL, 1, frame_n[l]);
      if (!corr[l])
        return 1;
    }

  printf ("=== corr (coherent cross-correlator, dwell = %d) ===\n", DWELL);
  printf ("%d rounds, min over rounds\n\n", ITERATIONS);

  DP_BENCH_SETTLE (
      (void)corr_execute (corr[0], in, frame_n[0], out, frame_n[0]));

  /* Rounds outside, (length, call kind) inside. The dump/accumulate ratio
     is the file's whole output, so both halves of it must see the same
     machine.
     Each timed pair starts from a fresh cycle: reset, then advance to one
     frame short of the dump so the next call is an accumulate and the one
     after it is the dump. The advance is untimed. */
  for (int r = 0; r < ITERATIONS; r++)
    for (int l = 0; l < N_LEN; l++)
      {
        const size_t n = frame_n[l];

        corr_reset (corr[l]);
        for (size_t f = 0; f + 2 < DWELL; f++)
          (void)corr_execute (corr[l], in, n, out, n);

        clock_gettime (CLOCK_MONOTONIC, &t0);
        (void)corr_execute (corr[l], in, n, out, n);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t[l * N_KIND + CFG_ACC][r] = dp_bench_elapsed (&t0, &t1);

        clock_gettime (CLOCK_MONOTONIC, &t0);
        (void)corr_execute (corr[l], in, n, out, n);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t[l * N_KIND + CFG_DUMP][r] = dp_bench_elapsed (&t0, &t1);
      }

  for (int l = 0; l < N_LEN; l++)
    for (int k = 0; k < N_KIND; k++)
      {
        (void)snprintf (name, sizeof name, "%s[n=%zu]", kind_name[k],
                        frame_n[l]);
        dp_bench_record (&_bench, name, t[l * N_KIND + k], ITERATIONS,
                         frame_n[l], "sample");
      }

  printf ("\n  what the deferred inverse buys, per frame:\n");
  printf ("    %-8s %10s", "n", "dump/acc");
  for (int d = 0; d < N_DWELL; d++)
    printf ("   dwell=%-4zu", dwells[d]);
  printf ("\n");
  for (int l = 0; l < N_LEN; l++)
    {
      const double acc  = dp_bench_min (t[l * N_KIND + CFG_ACC], ITERATIONS);
      const double dump = dp_bench_min (t[l * N_KIND + CFG_DUMP], ITERATIONS);
      printf ("    %-8zu %9.2fx", frame_n[l], dump / acc);
      for (int d = 0; d < N_DWELL; d++)
        printf ("  %7.2f ns", (acc * (double)(dwells[d] - 1) + dump)
                                  / (double)dwells[d] / (double)frame_n[l]
                                  * 1e9);
      printf ("\n");
    }
  printf ("  The dwell columns are ns/sample averaged over one cycle. A\n"
          "  non-coherent integrator pays the dwell=1 column at every\n"
          "  dwell, because it cannot defer the inverse at all.\n");

  for (int l = 0; l < N_LEN; l++)
    corr_destroy (corr[l]);
  free (ref);
  free (in);
  free (out);
  jm_bench_write_json (&_bench, "corr");
  return 0;
}
