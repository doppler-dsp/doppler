/*
 * test_acq_core.c — DSSS acquisition engine C-level tests.
 *
 * Covers: argument validation, auto-config (threshold/eta/dwell from the
 * (pfa, pd) target), and noise-free localization of a streamed burst to the
 * injected (Doppler bin, code phase).
 */
#include "acq/acq_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          _fails++;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

/* A length-7 maximal-length sequence (one period). */
static const uint8_t CODE7[7] = { 1, 1, 1, 0, 1, 0, 0 };

int
main (void)
{
  int          _fails = 0;
  const double PI     = acos (-1.0);

  const size_t sf = 7, spc = 2, ny = 8;
  const size_t nx = sf * spc; /* 14 */
  const size_t n  = ny * nx;  /* 112 */

  /* ── argument validation ────────────────────────────────────────────── */
  CHECK (acq_create (NULL, 0, sf, spc, ny, 1e-3, 0.9, 0.1, 0, 64) == NULL);
  CHECK (acq_create (CODE7, 6, sf, spc, ny, 1e-3, 0.9, 0.1, 0, 64)
         == NULL); /* code_len != sf */
  CHECK (acq_create (CODE7, 7, sf, spc, ny, 0.0, 0.9, 0.1, 0, 64)
         == NULL); /* pfa out of range */

  /* ── auto-config ────────────────────────────────────────────────────── */
  acq_state_t *a = acq_create (CODE7, 7, sf, spc, ny, 1e-2, 0.9, 1.0, 0, 64);
  CHECK (a != NULL);
  if (!a)
    return 1;
  CHECK (a->nx == nx);
  CHECK (a->n == n);
  CHECK (a->dwell == 1); /* min_snr=1.0 is easily detected in one frame */
  CHECK (a->noise_lo == 0 && a->noise_hi == n - 1);
  /* threshold = eta * sqrt(2/pi); eta = sqrt(-2 ln pfa_cell) > 0 */
  CHECK (a->eta > 0.0f);
  CHECK (fabsf (a->threshold - a->eta * 0.7978845608f) < 1e-4f);

  /* ── noise-free localization ────────────────────────────────────────── */
  const size_t u = 1;                             /* Doppler bin           */
  const size_t d = 5;                             /* code phase (samples)  */
  const double f = (double)u / (double)(nx * ny); /* carrier, f*nx*ny = u  */

  /* Oversampled, code-phase-rolled BPSK replica (one segment, length nx). */
  float complex s0d[14];
  for (size_t q = 0; q < nx; q++)
    {
      size_t  src  = (q + nx - (d % nx)) % nx; /* roll by +d */
      uint8_t chip = CODE7[(src / spc) % sf];
      s0d[q]       = (chip & 1u) ? -1.0f : 1.0f;
    }

  /* Tile ny segments with the continuous carrier; push the raw frame. */
  float complex *burst = malloc (n * sizeof (float complex));
  for (size_t k = 0; k < n; k++)
    {
      double ph = 2.0 * PI * f * (double)k;
      burst[k]  = s0d[k % nx] * (float complex) (cos (ph) + I * sin (ph));
    }

  acq_result_t hits[8];
  size_t       nh = acq_push (a, burst, n, hits, 8);
  CHECK (nh == 1); /* dwell=1: one frame -> one dump */
  if (nh == 1)
    {
      CHECK (hits[0].doppler_bin == u);
      CHECK (hits[0].code_phase == d);
      CHECK (hits[0].test_stat > a->threshold);
      CHECK (isfinite (hits[0].snr_est) && hits[0].snr_est > 0.0f);
    }

  /* reset drains the ring and clears the accumulator. */
  acq_reset (a);

  free (burst);
  acq_destroy (a);
  acq_destroy (NULL); /* must not crash */

  if (_fails)
    {
      fprintf (stderr, "test_acq_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_acq_core PASSED\n");
  return 0;
}
