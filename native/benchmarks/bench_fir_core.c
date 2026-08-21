/* bench_fir_core.c -- what a tap costs, and what a COMPLEX tap costs.
 *
 * `fir_execute` is the library's most-used inner loop, and it has two of
 * them: `inner_real_cf32` for a filter built by `fir_create_real`, and
 * `inner_cf32` for one built by `fir_create`. Both consume complex
 * samples; they differ only in whether each tap is a real scale or a
 * complex rotate-and-scale. The choice is made once, at construction,
 * by which constructor the caller reached for -- and nothing in the
 * `execute` signature says the two are not the same price.
 *
 * They are not, and the ratio is the row worth having. A complex tap is
 * four multiplies and two adds against one multiply and one add, so the
 * arithmetic bound is 4x; the SIMD paths (AVX-512 permute + sign mask,
 * AVX2, NEON) close most of that, and how much they close is a property
 * of the machine, not of the algorithm. A caller with real taps who
 * builds them with `fir_create` -- easy to do, since complex taps accept
 * a real array with zero imaginary parts -- pays that difference for
 * nothing, silently.
 *
 * Three lengths at both tap kinds, because the per-tap cost is not
 * constant either: at 15 taps the per-call overhead (two memcpys of the
 * delay line, the scratch check) is a visible share of the work, and by
 * 255 it is not. `ns/sample` divided by the tap count is what separates
 * the two effects.
 */
#include "dp_bench.h"
#include "fir/fir_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define BLOCK 65536
#define ITERATIONS 200
#define N_LEN 3

/* Odd lengths: a linear-phase FIR has an odd tap count, and the delay
   line is num_taps - 1, so these also exercise an even memcpy. */
static const size_t taps_n[N_LEN] = { 15, 63, 255 };

/* Two filters per length -- same coefficients, two representations. The
   complex one carries zero imaginary parts on purpose: it is the shape a
   caller gets by handing real taps to the complex constructor, which is
   the mistake this benchmark exists to price. */
enum
{
  CFG_REAL,
  CFG_COMPLEX,
  N_KIND
};

#define N_CFG (N_LEN * N_KIND)

static const char *kind_name[N_KIND] = { "real", "complex" };

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  static double   t[N_CFG][ITERATIONS];
  fir_state_t    *fir[N_CFG] = { 0 };
  float          *rtaps      = NULL;
  float complex  *ctaps = NULL, *in = NULL, *out = NULL;
  char            name[64];
  size_t          n_max = taps_n[N_LEN - 1];

  rtaps = malloc (n_max * sizeof *rtaps);
  ctaps = malloc (n_max * sizeof *ctaps);
  in    = malloc (BLOCK * sizeof *in);
  out   = malloc (BLOCK * sizeof *out);
  if (!rtaps || !ctaps || !in || !out)
    return 1;

  /* A windowed sinc rather than a flat array: real coefficients with a
     realistic mix of magnitudes and signs, so nothing here is a value the
     hardware can shortcut. */
  for (size_t k = 0; k < n_max; k++)
    {
      const double m = (double)k - (double)(n_max - 1) / 2.0;
      const double s = (m == 0.0) ? 1.0 : sin (M_PI * 0.25 * m) / (M_PI * m);
      rtaps[k]       = (float)(s
                               * (0.54
                                  - 0.46
                                        * cos (2.0 * M_PI * (double)k
                                               / (double)(n_max - 1))));
      ctaps[k]       = (float complex)rtaps[k];
    }

  for (size_t i = 0; i < BLOCK; i++)
    in[i] = (float complex) (cosf (0.01f * (float)i)
                             + I * sinf (0.013f * (float)i));

  for (int l = 0; l < N_LEN; l++)
    {
      fir[l * N_KIND + CFG_REAL]    = fir_create_real (rtaps, taps_n[l]);
      fir[l * N_KIND + CFG_COMPLEX] = fir_create (ctaps, taps_n[l]);
    }
  for (int c = 0; c < N_CFG; c++)
    if (!fir[c])
      return 1;

  printf ("=== fir (block FIR, %d samples per call) ===\n", BLOCK);
  printf ("%d rounds, min over rounds\n\n", ITERATIONS);

  DP_BENCH_SETTLE (fir_execute (fir[0], in, BLOCK, out));

  /* Rounds outside, configurations inside: the real/complex ratio is the
     point of the file, so a thermal step must land on both halves of it. */
  for (int r = 0; r < ITERATIONS; r++)
    for (int c = 0; c < N_CFG; c++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        fir_execute (fir[c], in, BLOCK, out);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t[c][r] = dp_bench_elapsed (&t0, &t1);
      }

  for (int l = 0; l < N_LEN; l++)
    for (int k = 0; k < N_KIND; k++)
      {
        const int c = l * N_KIND + k;
        (void)snprintf (name, sizeof name, "execute[taps=%zu,%s]", taps_n[l],
                        kind_name[k]);
        dp_bench_record (&_bench, name, t[c], ITERATIONS, BLOCK, "sample");
      }

  printf ("\n  complex-tap cost over real-tap, same coefficients:\n");
  for (int l = 0; l < N_LEN; l++)
    printf ("    %3zu taps  %.2fx      (%.3f vs %.3f ns/tap/sample)\n",
            taps_n[l],
            dp_bench_min (t[l * N_KIND + CFG_COMPLEX], ITERATIONS)
                / dp_bench_min (t[l * N_KIND + CFG_REAL], ITERATIONS),
            dp_bench_min (t[l * N_KIND + CFG_COMPLEX], ITERATIONS) * 1e9
                / (double)BLOCK / (double)taps_n[l],
            dp_bench_min (t[l * N_KIND + CFG_REAL], ITERATIONS) * 1e9
                / (double)BLOCK / (double)taps_n[l]);
  printf ("  4.0x is the arithmetic bound; what SIMD gives back is the\n"
          "  machine's answer, not the algorithm's. Read the two ns/tap\n"
          "  columns against each other rather than the ratio alone: they\n"
          "  need not move the same way with length, and where they meet,\n"
          "  the tap kind has stopped being what the call costs.\n");

  for (int c = 0; c < N_CFG; c++)
    fir_destroy (fir[c]);
  free (rtaps);
  free (ctaps);
  free (in);
  free (out);
  jm_bench_write_json (&_bench, "fir");
  return 0;
}
