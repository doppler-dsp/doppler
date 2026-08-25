#include "dp_test.h"
#include "ppe/ppe_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* y[m] = exp(j2π(f·m + ½·r·m²)) — a unit-amplitude linear chirp. */
static void
synth_chirp (float complex *y, size_t L, double f, double r)
{
  for (size_t m = 0; m < L; m++)
    {
      double ph
          = 2.0 * M_PI * (f * (double)m + 0.5 * r * (double)m * (double)m);
      y[m] = (float)cos (ph) + (float)sin (ph) * I;
    }
}

int
main (void)
{
  const size_t L    = 512;
  const float  ftol = 5e-3f, rtol = 5e-6f;

  /* Argument validation. */
  DP_CHECK (ppe_create (2, 0.0) == NULL);   /* max_len < 4 */
  DP_CHECK (ppe_create (64, -1.0) == NULL); /* negative rate span */

  float complex *y = malloc (L * sizeof *y);
  DP_CHECK (y != NULL);

  /* ── Doppler-only knob (max_rate = 0): a single FFT, rate forced to 0. ────
   */
  {
    ppe_state_t *p = ppe_create (L, 0.0);
    DP_CHECK (p != NULL && p->n_rate == 1);
    synth_chirp (y, L, 0.1, 0.0);
    ppe_result_t e = ppe_estimate (p, y, L);
    DP_CHECK (fabs (e.freq_norm - 0.1) <= ftol);
    DP_CHECK (e.rate_norm == 0.0);
    ppe_destroy (p);
  }

  /* ── Joint (frequency × chirp-rate) search via the coherent surface. ─────
   */
  {
    ppe_state_t *p = ppe_create (L, 5e-5);
    DP_CHECK (p != NULL && p->n_rate > 1);

    synth_chirp (y, L, 0.05, 1e-5); /* +freq, +chirp */
    ppe_result_t e = ppe_estimate (p, y, L);
    DP_CHECK (fabs (e.freq_norm - 0.05) <= ftol);
    DP_CHECK (fabs (e.rate_norm - 1e-5) <= rtol);

    synth_chirp (y, L, -0.08, -2e-5); /* sign handling */
    e = ppe_estimate (p, y, L);
    DP_CHECK (fabs (e.freq_norm + 0.08) <= ftol);
    DP_CHECK (fabs (e.rate_norm + 2e-5) <= rtol);

    synth_chirp (y, L, 0.12, 0.0); /* zero rate inside a rate search */
    e = ppe_estimate (p, y, L);
    DP_CHECK (fabs (e.freq_norm - 0.12) <= ftol);
    DP_CHECK (fabs (e.rate_norm) <= rtol);

    /* Out-of-range length → zeroed result, no crash. */
    e = ppe_estimate (p, y, L + 1);
    DP_CHECK (e.freq_norm == 0.0 && e.rate_norm == 0.0);
    ppe_destroy (p);
  }

  /* ── sub-bin refinement, at a tolerance that can actually see it ──────
   *
   * The header claims the peak is "refined sub-bin in both axes by
   * parabolic interpolation". The tolerances above cannot check that: at
   * L = 512 an FFT bin is 1/512 = 1.95e-3, so `ftol` of 5e-3 is 2.6 BINS
   * -- wide enough to pass with the interpolation deleted and the raw
   * argmax returned.
   *
   * Measured, the estimator resolves a noiseless tone to about 1e-4 of a
   * bin, so the gate below is set at 0.01 bins: a hundred times tighter
   * than the old tolerance and a hundred times looser than the object
   * achieves, which leaves headroom for a float32 transform without
   * leaving room for the refinement to vanish. */
  {
    ppe_state_t *p = ppe_create (L, 0.0);
    DP_CHECK (p != NULL);
    const double binw = 1.0 / (double)L;
    for (int k = 0; k < 8; k++)
      {
        double frac = (double)k / 8.0;
        double f    = (40.0 + frac) * binw;
        synth_chirp (y, L, f, 0.0);
        ppe_result_t e = ppe_estimate (p, y, L);
        DP_CHECK (fabs (e.freq_norm - f) < 0.01 * binw);
      }
    ppe_destroy (p);
  }

  /* ── snr_db carries the COHERENT PROCESSING GAIN ──────────────────────
   *
   * snr_db is documented only as a "winning-row peak-to-mean (rough
   * confidence)" and was checked by nothing at all. It is a peak-to-mean
   * measured AFTER the coherent transform, so it reads roughly
   * 10*log10(L) above the input SNR -- 27 dB at L = 512. A caller
   * thresholding on it without knowing that is comparing an integrated
   * quantity against an input-referred number.
   *
   * Asserted as the RELATIONSHIP, not a literal: doubling the segment
   * length must add about 3 dB on the same input. A literal would pin the
   * noise draw as much as the estimator. */
  {
    const size_t   la = 256, lb = 1024;
    float complex *ya = malloc (lb * sizeof *ya);
    DP_CHECK (ya != NULL);
    if (ya)
      {
        double snr[2];
        size_t lens[2] = { la, lb };
        for (int i = 0; i < 2; i++)
          {
            ppe_state_t *p = ppe_create (lens[i], 0.0);
            DP_CHECK (p != NULL);
            synth_chirp (ya, lens[i], 0.05, 0.0);
            ppe_result_t e = ppe_estimate (p, ya, lens[i]);
            snr[i]         = e.snr_db;
            DP_CHECK (e.snr_db > 0.0);
            ppe_destroy (p);
          }
        /* 4x the length is 6.02 dB of coherent gain; allow a wide band
           because the mean of the surface is not exactly the noise floor
           for a noiseless input. */
        DP_CHECK (snr[1] - snr[0] > 3.0);
        DP_CHECK (snr[1] > snr[0]);
        free (ya);
      }
  }

  /* ── the documented input-length floor, and the range of freq_norm ────
   *
   * n_in is documented as `[4, max_len]` and only the upper bound was
   * checked. Below the floor every field must be zeroed rather than the
   * estimator running on a segment too short to have a spectrum.
   *
   * freq_norm is documented as `[-0.5, 0.5)`. Checked at both shoulders,
   * where an off-by-one in the bin-to-frequency mapping would wrap a
   * near-Nyquist tone to the wrong sign -- the failure that reads as a
   * receiver locking to the negative image. */
  {
    ppe_state_t *p = ppe_create (L, 0.0);
    DP_CHECK (p != NULL);

    synth_chirp (y, L, 0.05, 0.0);
    for (size_t n = 0; n < 4; n++)
      {
        ppe_result_t e = ppe_estimate (p, y, n);
        DP_CHECK (e.freq_norm == 0.0 && e.rate_norm == 0.0 && e.snr_db == 0.0);
      }
    /* ... and the floor itself works, so the check above is a boundary
       and not a blanket refusal. */
    ppe_result_t ok = ppe_estimate (p, y, 4);
    DP_CHECK (ok.snr_db != 0.0);

    const double edges[4] = { 0.45, 0.499, -0.45, -0.499 };
    for (int i = 0; i < 4; i++)
      {
        synth_chirp (y, L, edges[i], 0.0);
        ppe_result_t e = ppe_estimate (p, y, L);
        DP_CHECK (e.freq_norm >= -0.5 && e.freq_norm < 0.5);
        DP_CHECK (fabs (e.freq_norm - edges[i]) < 1e-4);
        /* The sign survives: a wrapped estimate would land on the far
           shoulder, which |diff| < 1e-4 already excludes, but assert it
           directly so the intent is readable. */
        DP_CHECK ((e.freq_norm > 0.0) == (edges[i] > 0.0));
      }
    ppe_destroy (p);
  }

  /* ── nfft is 4x next_pow2(max_len), and the header said otherwise ─────
   *
   * The struct field was documented as "next pow2 of max_len" while the
   * implementation uses `next_pow2 (max_len) << 2`. That is not a cosmetic
   * slip: nfft sizes `buf`, `spec` and `mag`, so a caller budgeting memory
   * from the header was out by 4x, and the same 4x is what makes the
   * sub-bin refinement above as accurate as it is. Pinned so the comment
   * and the code cannot drift apart again. */
  {
    const size_t lens[3] = { 100, 512, 800 };
    for (int i = 0; i < 3; i++)
      {
        ppe_state_t *p = ppe_create (lens[i], 0.0);
        DP_CHECK (p != NULL);
        size_t np2 = 1;
        while (np2 < lens[i])
          np2 <<= 1;
        DP_CHECK (p->nfft == np2 * 4);
        DP_CHECK (p->max_len == lens[i]);
        ppe_destroy (p);
      }
  }

  /* ── reset really is a no-op ──────────────────────────────────────────
   *
   * The header says the estimator keeps no running state and that reset
   * exists only to satisfy the common object protocol. Asserted rather
   * than assumed: an estimate either side of a reset must be bit-identical,
   * which also rules out a reset that clears scratch the next estimate
   * depends on. */
  {
    ppe_state_t *p = ppe_create (L, 5e-5);
    DP_CHECK (p != NULL);
    synth_chirp (y, L, 0.077, 8e-6);
    ppe_result_t a = ppe_estimate (p, y, L);
    ppe_reset (p);
    ppe_result_t b = ppe_estimate (p, y, L);
    DP_CHECK (a.freq_norm == b.freq_norm);
    DP_CHECK (a.rate_norm == b.rate_norm);
    DP_CHECK (a.snr_db == b.snr_db);
    ppe_reset (NULL); /* must not crash */
    ppe_destroy (p);
  }

  free (y);
  DP_TEST_END ("test_ppe_core");
}
