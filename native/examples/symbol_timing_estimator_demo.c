/**
 * symbol_timing_estimator_demo.c — symbol timing from one block, no loop.
 *
 * A timing loop pulls in over a time that depends on where it started. The
 * feedforward estimator reads the timing off one block of matched-filtered
 * samples instead, in the block's own length, and it uses only the
 * stream's power, so it runs before the carrier is recovered.
 *
 * Demonstrates:
 *   1. Making BPSK with a rectangular pulse, 8 samples per symbol, on a
 *      carrier that is off frequency.
 *   2. Matched filtering it with a one-symbol running sum.
 *   3. One call to the estimator, and reading the eye center, the quality
 *      and the block length from the record it returns.
 *
 * It checks its own result. A running sum over a symbol peaks on the
 * symbol's last sample, which is 7/8 of a symbol on the estimator's clock,
 * or -1/8; the program exits 1 if the estimate is not within a hundredth
 * of a symbol of that.
 *
 * Build:
 *   make build
 *   ./build/native/examples/symbol_timing_estimator_demo
 */

#include <doppler/timing_est/timing_est_core.h>

#include "doppler/dp_complex.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define SPS 8     /* samples per symbol                  */
#define N_SYM 512 /* symbols in the block                */
#define N (SPS * N_SYM)
#define CARRIER 1e-3 /* carrier offset, cycles per sample   */

int
main (void)
{
  printf ("=== SymbolTimingEstimator Demo ===\n\n");

  /* The transmitter: random BPSK, each symbol held for SPS samples, on a
     carrier CARRIER cycles a sample off. */
  static float _Complex x[N], y[N];
  uint32_t lfsr = 0xACE1u;
  float    bit  = 1.0f;
  for (int i = 0; i < N; i++)
    {
      if (i % SPS == 0)
        {
          lfsr = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
          bit  = (lfsr & 1u) ? 1.0f : -1.0f;
        }
      double a = 6.283185307179586 * CARRIER * (double)i;
      x[i]     = bit * CMPLXF ((float)cos (a), (float)sin (a));
    }

  /* The matched filter for a rectangle: the sum over the last symbol. */
  for (int i = 0; i < N; i++)
    {
      float _Complex acc = 0.0f;
      for (int k = 0; k < SPS && k <= i; k++)
        acc += x[i - k];
      y[i] = acc / (float)SPS;
    }

  /* The estimator runs a symbol clock at SPS samples a symbol and reports
     where on that clock the stream's power peaks. */
  dp_timing_est_state_t *te = dp_timing_est_create ((double)SPS);
  if (!te)
    {
      fprintf (stderr, "ERROR: dp_timing_est_create failed\n");
      return 1;
    }
  dp_timing_estimate_t e = dp_timing_est_estimate (te, y, N);

  double want = -1.0 / SPS; /* the symbol's last sample, on the clock */
  printf ("Block:       %.0f symbols, mean power %.3f\n", e.symbols, e.power);
  printf ("Eye center:  %+.4f symbols on the clock (expected %+.4f)\n", e.tau,
          want);
  printf ("Quality:     %.3f\n", e.quality);

  int ok = fabs (e.tau - want) < 0.01 && e.quality > 0.0;
  printf ("\n%s\n", ok ? "PASS" : "FAIL");

  dp_timing_est_destroy (te);
  return ok ? 0 : 1;
}
