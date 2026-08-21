/* bench_hbdecim_q15_core.c -- what a Q15 halfband tap costs, and the
 * alignment step that is NOT the whole answer.
 *
 * The component holds its FIR-branch coefficients zero-padded to `K_pad`:
 * the symmetric pair count `K = num_taps / 2`, rounded UP to a multiple of
 * 16 so the SIMD loads stay aligned. Padding a coefficient array is
 * ordinarily just storage; here the natural reading is that it is
 * multiplies, and that the cost is therefore a STAIRCASE -- flat within a
 * step, jumping at every 32nd tap, and identical for any two lengths that
 * pad to the same width.
 *
 * That reading is what this file was written to check, and it does not
 * survive. Four lengths straddle two boundaries:
 *
 *   num_taps  32 -> K 16 -> K_pad 16     the cheap end of a step
 *   num_taps  34 -> K 17 -> K_pad 32     one tap later, one step up
 *   num_taps  64 -> K 32 -> K_pad 32     30 taps later, same padded width
 *   num_taps  66 -> K 33 -> K_pad 48     the next step
 *
 * 34 and 64 pad to the same `K_pad` and do not cost the same. So the
 * padding is not what is being paid for: cost rises with the taps the
 * caller actually passed, with a visible extra bump where a boundary is
 * crossed. Both effects are real and this file separates them rather than
 * explaining them -- the mechanism behind the bump (the padded multiplies,
 * the ring capacity, which is `next_pow2(num_taps)` and also steps there,
 * or the coefficient array leaving a cache line) is not isolated by these
 * four rows and is not claimed here.
 *
 * What a caller can take from it is the useful half: adding taps costs
 * roughly in proportion to the taps added, and crossing a 32-tap boundary
 * costs more than the taps that crossed it. Sizing a halfband branch just
 * under a boundary is worth doing; expecting the next 30 taps to be free
 * afterwards is not.
 *
 * Everything is per INPUT pair, not per output. A 2:1 decimator emits half
 * what it consumes, and it is the consumed rate that a front end has to
 * keep up with.
 */
#include "dp_bench.h"
#include "hbdecim_q15/hbdecim_q15_core.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define ITERATIONS 100
#define BLOCK 32768
#define N_CFG 4

static const size_t branch_taps[N_CFG] = { 32, 34, 64, 66 };
#define REF_IDX 0

/* K_pad as the component computes it -- printed beside each row so the
   staircase is visible rather than inferred. */
static size_t
k_pad_for (size_t num_taps)
{
  const size_t k = num_taps / 2;
  return ((k + 15) / 16) * 16;
}

int
main (void)
{
  jm_bench_t           _bench = { 0 };
  struct timespec      t0, t1;
  static double        t[N_CFG][ITERATIONS];
  hbdecim_q15_state_t *dec[N_CFG] = { 0 };
  float               *h          = NULL;
  int16_t             *in = NULL, *out = NULL;
  char                 name[72];
  const size_t         n_max = branch_taps[N_CFG - 1];

  h   = malloc (n_max * sizeof *h);
  in  = malloc ((size_t)2 * BLOCK * sizeof *in);
  out = malloc ((size_t)2 * BLOCK * sizeof *out);
  if (!h || !in || !out)
    return 1;

  /* Interleaved I/Q well below full scale, so nothing saturates and every
     row runs the same arithmetic. */
  for (size_t i = 0; i < BLOCK; i++)
    {
      in[2 * i]     = (int16_t)(8000.0 * cos (0.031 * (double)i));
      in[2 * i + 1] = (int16_t)(8000.0 * sin (0.031 * (double)i));
    }

  for (int c = 0; c < N_CFG; c++)
    {
      /* A symmetric windowed sinc: create() requires h[k] == h[n-1-k], and
         the values only have to be representative -- the cost does not
         depend on them, which is part of what this file is checking. */
      const size_t n = branch_taps[c];
      for (size_t k = 0; k < n; k++)
        {
          const double m = (double)k - (double)(n - 1) / 2.0;
          const double s = (fabs (m) < 1e-12)
                               ? 1.0
                               : sin (M_PI * 0.5 * m) / (M_PI * 0.5 * m);
          h[k]           = (float)(0.5 * s
                                   * (0.54
                                      - 0.46
                                            * cos (2.0 * M_PI * (double)k
                                                   / (double)(n - 1))));
        }
      dec[c] = hbdecim_q15_create (n, h);
      if (!dec[c])
        return 1;
    }

  printf ("=== hbdecim_q15 (Q15 halfband 2:1 decimator) ===\n");
  printf ("%d input pairs per call, %d rounds, min over rounds\n\n", BLOCK,
          ITERATIONS);

  DP_BENCH_SETTLE (
      (void)hbdecim_q15_execute (dec[REF_IDX], in, BLOCK, out, BLOCK));

  /* Rounds outside, lengths inside: the whole result is four rows read
     against each other, so a thermal step must not land on one length. */
  for (int r = 0; r < ITERATIONS; r++)
    for (int c = 0; c < N_CFG; c++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        (void)hbdecim_q15_execute (dec[c], in, BLOCK, out, BLOCK);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t[c][r] = dp_bench_elapsed (&t0, &t1);
      }

  for (int c = 0; c < N_CFG; c++)
    {
      (void)snprintf (name, sizeof name, "execute[taps=%zu,K_pad=%zu]",
                      branch_taps[c], k_pad_for (branch_taps[c]));
      dp_bench_record (&_bench, name, t[c], ITERATIONS, BLOCK, "in-pair");
    }

  printf ("\n  the staircase (taps=%zu = 1.00x):\n", branch_taps[REF_IDX]);
  for (int c = 0; c < N_CFG; c++)
    printf ("    taps=%-3zu  K_pad=%-3zu  %.2fx   %6.4f ns/pair/padded-tap\n",
            branch_taps[c], k_pad_for (branch_taps[c]),
            dp_bench_min (t[c], ITERATIONS)
                / dp_bench_min (t[REF_IDX], ITERATIONS),
            dp_bench_min (t[c], ITERATIONS) / (double)BLOCK
                / (double)k_pad_for (branch_taps[c]) * 1e9);
  printf ("  The last column divides by K_pad, so it would be FLAT if the\n"
          "  padded width were what the loop costs. Compare the two rows\n"
          "  that share a K_pad: they are the test, and a gap between them\n"
          "  is the padding NOT being the price. Read the ratio column\n"
          "  instead -- it says what adding taps costs, and shows the\n"
          "  boundary charging more than the taps that crossed it.\n");

  for (int c = 0; c < N_CFG; c++)
    hbdecim_q15_destroy (dec[c]);
  free (h);
  free (in);
  free (out);
  jm_bench_write_json (&_bench, "hbdecim_q15");
  return 0;
}
