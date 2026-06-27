/*
 * test_acq_core.c — DSSS acquisition engine C-level tests.
 *
 * Covers: argument validation, physics auto-config (C/N0 -> snr, the chosen
 * coherent depth doppler_bins, threshold/eta), and noise-free localization of
 * a streamed burst to the injected (Doppler bin, code phase).
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

  const size_t spc   = 2;
  const size_t nx    = 7 * spc; /* code_bins = sf*spc = 14 */
  const double crate = 1.0e6;   /* 1 MHz chips */
  const double span  = crate / (2.0 * 7.0);

  /* ── argument validation ────────────────────────────────────────────── */
  CHECK (acq_create (NULL, 0, 8, spc, crate, 45.0, 0.0, 1e-3, 0.9, 0, 1)
         == NULL);
  CHECK (acq_create (CODE7, 7, 8, spc, 0.0, 45.0, 0.0, 1e-3, 0.9, 0, 1)
         == NULL); /* chip_rate <= 0 */
  CHECK (acq_create (CODE7, 7, 8, spc, crate, 0.0, 0.0, 1e-3, 0.9, 0, 1)
         == NULL); /* cn0_dbhz <= 0 */
  CHECK (acq_create (CODE7, 7, 8, spc, crate, 45.0, 0.0, 0.0, 0.9, 0, 1)
         == NULL); /* pfa out of range */
  CHECK (
      acq_create (CODE7, 7, 8, spc, crate, 45.0, span * 2.0, 1e-3, 0.9, 0, 1)
      == NULL); /* doppler_uncertainty > span */

  /* ── auto-config: a strong C/N0 needs only one coherent rep ──────────── */
  acq_state_t *a
      = acq_create (CODE7, 7, 8, spc, crate, 65.0, 0.0, 1e-2, 0.9, 0, 1);
  CHECK (a != NULL);
  if (!a)
    return 1;
  CHECK (a->sf == 7);
  CHECK (a->code_bins == nx);
  CHECK (a->doppler_bins == 1); /* 65 dB-Hz is detected in a single rep */
  CHECK (a->n == a->doppler_bins * nx);
  CHECK (!a->underpowered && a->pd_predicted >= 0.9);
  CHECK (a->noise_lo == 0 && a->noise_hi == a->n - 1);
  CHECK (a->fs == crate * (double)spc);
  CHECK (fabs (a->doppler_span_hz - span) < 1e-6);
  /* threshold = eta * sqrt(2/pi); eta = sqrt(-2 ln pfa_cell) > 0 */
  CHECK (a->eta > 0.0f);
  CHECK (fabsf (a->threshold - a->eta * 0.7978845608f) < 1e-4f);
  acq_destroy (a);

  /* ── noise-free localization (force a multi-bin Doppler axis) ─────────── */
  /* A very weak target C/N0 makes the D-search exhaust to reps, so
   * doppler_bins == reps and the slow-time axis has bins to localize on.  The
   * injected burst is noise-free, so it clears the (best-effort) gate anyway.
   */
  acq_state_t *b
      = acq_create (CODE7, 7, 8, spc, crate, 20.0, 0.0, 1e-2, 0.9, 0, 1);
  CHECK (b != NULL);
  if (!b)
    return 1;
  CHECK (b->doppler_bins == 8); /* exhausted to reps */
  const size_t ny = b->doppler_bins;
  const size_t n  = b->n; /* ny * nx */

  const size_t u = 1;                             /* Doppler bin           */
  const size_t d = 5;                             /* code phase (samples)  */
  const double f = (double)u / (double)(nx * ny); /* carrier, f*nx*ny = u  */

  /* Oversampled, code-phase-rolled BPSK replica (one segment, length nx). */
  float complex s0d[14];
  for (size_t q = 0; q < nx; q++)
    {
      size_t  src  = (q + nx - (d % nx)) % nx; /* roll by +d */
      uint8_t chip = CODE7[(src / spc) % 7];
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
  size_t       nh = acq_push (b, burst, n, hits, 8);
  CHECK (nh == 1); /* one frame -> one dump */
  if (nh == 1)
    {
      CHECK (hits[0].doppler_bin == u);
      CHECK (hits[0].code_phase == d);
      CHECK (hits[0].test_stat > b->threshold);
      CHECK (isfinite (hits[0].snr_est) && hits[0].snr_est > 0.0f);
    }

  /* reset drains the ring and clears the accumulator. */
  acq_reset (b);

  free (burst);
  acq_destroy (b);
  acq_destroy (NULL); /* must not crash */

  /* ── carry round-trip: split a stream across two engines ─────────────────
   * A fresh engine + the carry blob must reproduce an uninterrupted run
   * exactly — the elastic-resume (pod handoff) guarantee. */
  {
    acq_state_t *ra
        = acq_create (CODE7, 7, 8, spc, crate, 20.0, 0.0, 1e-2, 0.9, 0, 1);
    CHECK (ra != NULL);
    if (ra)
      {
        const size_t rn  = ra->n;       /* frame size (ny*nx)            */
        const size_t L3  = 3 * rn + 5;  /* 3 full frames + a partial tail */
        const size_t cut = rn + rn / 2; /* split mid-frame (1.5 frames)   */
        const double rf  = 1.0 / (double)rn; /* Doppler bin u=1 (rf*rn = 1)  */

        float complex *s = malloc (L3 * sizeof (float complex));
        for (size_t k = 0; k < L3; k++)
          {
            double ph = 2.0 * PI * rf * (double)k;
            s[k] = s0d[k % nx] * (float complex) (cos (ph) + I * sin (ph));
          }

        /* Run A — uninterrupted. */
        acq_result_t hA[8];
        size_t       nA = acq_push (ra, s, L3, hA, 8);

        /* Run B — engine1 takes [0,cut), hands its carry to a fresh engine2
         * which takes [cut,L3). */
        acq_state_t *r1
            = acq_create (CODE7, 7, 8, spc, crate, 20.0, 0.0, 1e-2, 0.9, 0, 1);
        acq_state_t *r2
            = acq_create (CODE7, 7, 8, spc, crate, 20.0, 0.0, 1e-2, 0.9, 0, 1);
        CHECK (r1 && r2);
        if (r1 && r2)
          {
            acq_result_t hB[8];
            size_t       nB = acq_push (r1, s, cut, hB, 8);

            size_t cb    = acq_carry_bytes (r1);
            void  *carry = malloc (cb);
            acq_get_carry (r1, carry);
            CHECK (acq_set_carry (r2, carry) == 0);

            nB += acq_push (r2, s + cut, L3 - cut, hB + nB, 8 - nB);

            CHECK (nA == 3 && nB == nA); /* both see all 3 full frames */
            for (size_t i = 0; i < nA && i < nB; i++)
              {
                CHECK (hA[i].doppler_bin == hB[i].doppler_bin);
                CHECK (hA[i].code_phase == hB[i].code_phase);
                CHECK (fabsf (hA[i].peak_mag - hB[i].peak_mag) < 1e-5f);
                CHECK (fabsf (hA[i].test_stat - hB[i].test_stat) < 1e-5f);
                CHECK (fabsf (hA[i].snr_est - hB[i].snr_est) < 1e-5f);
              }
            free (carry);
          }
        acq_destroy (r1);
        acq_destroy (r2);
        free (s);
      }
    acq_destroy (ra);
  }

  if (_fails)
    {
      fprintf (stderr, "test_acq_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_acq_core PASSED\n");
  return 0;
}
