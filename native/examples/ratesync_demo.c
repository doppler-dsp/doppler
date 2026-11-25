/**
 * ratesync_demo.c — symbol timing recovery on a rectangular pulse.
 *
 * RateSync is a matched filter, a fractional resampler and a timing loop in
 * one object: its matched bank is the interpolator, so the loop steers the
 * sampling instant by choosing an arm.
 *
 * Demonstrates:
 *   1. Creating a RateSync for a rectangular (NRZ) pulse at 4 input
 *      samples per symbol, delivering one recovered symbol per
 *      transmitted symbol.
 *   2. Feeding it BPSK whose first symbol starts three samples late.
 *   3. Reading back what the loop found: its lock statistic and decision,
 *      the samples per symbol it tracked, and how open the eye is at the
 *      recovered symbols.
 *
 * It checks its own result: the loop must declare lock and every symbol
 * of the second half must land within 10% of +1 or -1, or the program
 * exits 1.
 *
 * Build:
 *   make build
 *   ./build/native/examples/ratesync_demo
 */

#include <doppler/ratesync/ratesync_core.h>

#include "doppler/dp_complex.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SPS 4.0     /* input samples per symbol                      */
#define TAU 0.75    /* where the first symbol starts, in symbols     */
#define M 4         /* samples per symbol the timing detector works at */
#define N_SYM 20000 /* symbols transmitted                           */

int
main (void)
{
  printf ("=== RateSync Demo ===\n\n");

  /* The transmitter: random BPSK symbols held for one symbol each. A
     symbol lasts SPS input samples and the first one starts TAU of a
     symbol late, so sample i belongs to symbol floor(i / SPS + 1 - TAU). */
  size_t          n   = (size_t)((N_SYM - 1) * SPS);
  float _Complex *x   = malloc (n * sizeof *x);
  float _Complex *y   = malloc (n * sizeof *y);
  float          *bit = malloc (N_SYM * sizeof *bit);
  if (!x || !y || !bit)
    return 1;
  uint32_t lfsr = 0xACE1u; /* any nonzero seed */
  for (size_t k = 0; k < N_SYM; k++)
    {
      lfsr   = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      bit[k] = (lfsr & 1u) ? 1.0f : -1.0f;
    }
  for (size_t i = 0; i < n; i++)
    x[i] = bit[(size_t)((double)i / SPS + 1.0 - TAU)];

  /* 1024 arms, a loop bandwidth of 1% of the symbol rate, damping 0.707,
     the Gardner detector. */
  dp_ratesync_state_t *rs
      = dp_ratesync_create (SPS, RATESYNC_PULSE_IANDD, 0.0, 1, M, 1024, 0.01,
                            0.707, RATESYNC_TED_GARDNER);
  if (!rs)
    {
      fprintf (stderr, "ERROR: dp_ratesync_create failed\n");
      return 1;
    }

  /* One call: every input in, one recovered symbol per symbol out. */
  size_t n_out = dp_ratesync_steps (rs, x, n, y, n);

  double lock   = dp_ratesync_get_lock_stat (rs);
  int    locked = dp_ratesync_get_locked (rs);

  /* The eye at the recovered symbols, once the loop has settled: a symbol
     sampled at its center comes back as +1 or -1. */
  double worst = 0.0;
  for (size_t k = n_out / 2; k < n_out; k++)
    {
      double miss = fabs (fabs ((double)crealf (y[k])) - 1.0);
      worst       = miss > worst ? miss : worst;
    }

  printf ("Input:   %zu samples, %.1f samples per symbol, first symbol "
          "%.2f of a symbol late\n",
          n, SPS, TAU);
  printf ("Output:  %zu symbols\n\n", n_out);
  printf ("Lock statistic:          %.3f (%s)\n", lock,
          locked ? "locked" : "not locked");
  printf ("Tracked samples/symbol:  %.5f\n", dp_ratesync_get_rate (rs));
  printf ("Worst symbol, 2nd half:  %.4f from +-1\n", worst);

  int ok = locked && worst < 0.1;
  printf ("\n%s\n", ok ? "PASS" : "FAIL");

  dp_ratesync_destroy (rs);
  free (x);
  free (y);
  free (bit);
  return ok ? 0 : 1;
}
