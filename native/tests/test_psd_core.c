#include "doppler/dp_complex.h"
#include "doppler/psd/psd_core.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* argmax over a float array. */
static size_t
argmax (const float *a, size_t n)
{
  size_t m = 0;
  for (size_t i = 1; i < n; i++)
    if (a[i] > a[m])
      m = i;
  return m;
}

/* Fill x[0..n-1] with a unit-amplitude complex tone at FFT bin k. */
static void
fill_tone (float _Complex *x, size_t n, int k)
{
  for (size_t i = 0; i < n; i++)
    {
      double ph = 2.0 * M_PI * (double)k * (double)i / (double)n;
      x[i]      = (float _Complex) (cos (ph) + sin (ph) * I);
    }
}

/* Fill x[0..n-1] with a real cosine of amplitude A at FFT bin k. */
static void
fill_real_tone (float *x, size_t n, int k, double a)
{
  for (size_t i = 0; i < n; i++)
    x[i] = (float)(a * cos (2.0 * M_PI * (double)k * (double)i / (double)n));
}

/* Deterministic [-1,1] sample from a tiny LCG (portable, seeded). */
static float
lcg_unit (uint64_t *st)
{
  *st = *st * 6364136223846793005ULL + 1442695040888963407ULL;
  return (float)((double)(*st >> 33) / (double)(1ULL << 31) - 1.0);
}

/* Population standard deviation of a[lo..hi). */
static double
stddev (const float *a, size_t lo, size_t hi)
{
  double mean = 0.0;
  for (size_t i = lo; i < hi; i++)
    mean += a[i];
  mean /= (double)(hi - lo);
  double var = 0.0;
  for (size_t i = lo; i < hi; i++)
    var += (a[i] - mean) * (a[i] - mean);
  return sqrt (var / (double)(hi - lo));
}

int
main (void)
{
  const size_t N = 64;

  /* ── lifecycle + invalid args ───────────────────────────────────────── */
  {
    DP_CHECK (dp_psd_create (1, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1)
              == NULL); /* n<2  */
    DP_CHECK (dp_psd_create (N, 0.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1)
              == NULL); /* fs   */
    DP_CHECK (dp_psd_create (N, 1.0, 4, 0.0f, 1, 1.0, 0, 0, 0.1)
              == NULL); /* win: 0..3 are Hann, Kaiser, B-H, rect */
    DP_CHECK (dp_psd_create (N, 1.0, 0, 0.0f, 1, 0.0, 0, 0, 0.1)
              == NULL); /* fscl */
    DP_CHECK (dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 9, 0.1)
              == NULL);    /* mode */
    dp_psd_destroy (NULL); /* ok   */

    dp_psd_state_t *w = dp_psd_create (N, 1.0e6, 1, 8.0f, 1, 1.0, 0, 0, 0.1);
    DP_CHECK (w != NULL);
    DP_CHECK (w->n == N);
    DP_CHECK (w->fs == 1.0e6);
    DP_CHECK (w->enbw > 1.0); /* any non-rectangular window has ENBW > 1 bin */

    /* psd_db before any frame → 0 (None in Python). */
    float db[64];
    DP_CHECK (dp_psd_psd_db (w, N, db, N) == 0);
    dp_psd_destroy (w);
  }

  /* ── refusals the certification found missing (#1911 (b), (c), (f)) ────
   * Each beside its precondition -- the nearest valid value is accepted --
   * so a create that refused everything could not pass. */
  {
    /* (b) pad 0 was silently treated as 1; it is refused like n < 2. */
    DP_CHECK (dp_psd_create (N, 1.0, 0, 0.0f, 0, 1.0, 0, 0, 0.1) == NULL);
    dp_psd_state_t *p1 = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    DP_CHECK (p1 != NULL);
    dp_psd_destroy (p1);

    /* (c) exp mode needs alpha in (0, 1].  The rule is the averager's:
     * dp_acc_trace_create refuses, and PSD returns its NULL.  Outside it
     * the EMA is not an average -- 0 never left the first frame, -0.5 read
     * negative power, 1.5 saturates to pass-through (dp_ema_step), NaN
     * poisoned every bin.  The other three modes never read alpha, so each
     * still accepts it. */
    const double bad[] = { 0.0, -0.5, 1.5, NAN };
    const int    no_read[]
        = { ACC_TRACE_MEAN, ACC_TRACE_MAXHOLD, ACC_TRACE_MINHOLD };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++)
      {
        DP_CHECK (
            dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, ACC_TRACE_EXP, bad[i])
            == NULL);
        for (size_t k = 0; k < sizeof no_read / sizeof no_read[0]; k++)
          {
            dp_psd_state_t *m = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0,
                                               no_read[k], bad[i]);
            DP_CHECK (m != NULL);
            dp_psd_destroy (m);
          }
      }
    const double good[] = { 1.0, 0.25, 1e-9 };
    for (size_t i = 0; i < sizeof good / sizeof good[0]; i++)
      {
        dp_psd_state_t *e = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0,
                                           ACC_TRACE_EXP, good[i]);
        DP_CHECK (e != NULL);
        dp_psd_destroy (e);
      }

    /* (f) a window with zero coherent gain: the symmetric Hann at n = 2 is
     * [0, 0], and every reading divides by sum(w)^2.  It read -200 dB from
     * psd_db and NaN from psd_dbhz for any input.  Blackman-Harris at n = 2
     * sums to 1.2e-4 and Hann at n = 3 to 1: tiny or small, not zero. */
    DP_CHECK (dp_psd_create (2, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1) == NULL);
    dp_psd_state_t *bh2 = dp_psd_create (2, 1.0, 2, 0.0f, 1, 1.0, 0, 0, 0.1);
    dp_psd_state_t *h3  = dp_psd_create (3, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    DP_CHECK (bh2 != NULL && bh2->cg > 0.0);
    DP_CHECK (h3 != NULL && h3->cg > 0.0);
    dp_psd_destroy (bh2);
    dp_psd_destroy (h3);

    /* A size no buffer can hold is refused, not wrapped.  n = 2^62 asked
     * malloc for n * 4 = 0 bytes, got a pointer, and the window fill ran off
     * it.  n = 2, pad = 2^63 wrapped n * pad to 0, read nfft = 1, and was
     * accepted with a frame longer than its buffer.  n = 3, pad = 2^62 has
     * no power of two above it.  Rect, so no window refuses first.  Each is
     * refused before any allocation, which the sanitizer legs require: ASan
     * and TSan report an overflowing calloc as an error, not a NULL. */
    const size_t big = (size_t)1 << 62;
    DP_CHECK (dp_psd_create (big, 1.0, 3, 0.0f, 1, 1.0, 0, 0, 0.1) == NULL);
    DP_CHECK (dp_psd_create (2, 1.0, 3, 0.0f, 2 * big, 1.0, 0, 0, 0.1)
              == NULL);
    DP_CHECK (dp_psd_create (3, 1.0, 3, 0.0f, big, 1.0, 0, 0, 0.1) == NULL);
    /* precondition: n = 2 under rect, at an ordinary pad, still builds */
    dp_psd_state_t *r2 = dp_psd_create (2, 1.0, 3, 0.0f, 4, 1.0, 0, 0, 0.1);
    DP_CHECK (r2 != NULL && r2->nfft == 8);
    dp_psd_destroy (r2);

    /* What every reading divides by must be a finite positive number.  A
     * NaN passed a plain `<= 0.0` and built an estimator reading NaN or
     * -200 dB: fs and full_scale NaN or inf; a Kaiser beta of NaN, or from
     * about 2.3e5 up, where I0 overflows and every tap is NaN; and bits
     * past 64, whose reference outgrows any sample format (and whose
     * (int)bits - 1 was undefined past INT_MAX).  Each beside its nearest
     * accepted value. */
    const double nonfinite[] = { NAN, INFINITY };
    for (size_t i = 0; i < 2; i++)
      {
        DP_CHECK (dp_psd_create (N, nonfinite[i], 0, 0.0f, 1, 1.0, 0, 0, 0.1)
                  == NULL);
        DP_CHECK (dp_psd_create (N, 1.0, 0, 0.0f, 1, nonfinite[i], 0, 0, 0.1)
                  == NULL);
      }
    const float bad_beta[] = { NAN, INFINITY, 2.3e5f };
    for (size_t i = 0; i < 3; i++)
      {
        DP_CHECK (dp_psd_create (N, 1.0, 1, bad_beta[i], 1, 1.0, 0, 0, 0.1)
                  == NULL);
        /* beta is Kaiser's alone: Hann ignores it */
        dp_psd_state_t *h
            = dp_psd_create (N, 1.0, 0, bad_beta[i], 1, 1.0, 0, 0, 0.1);
        DP_CHECK (h != NULL);
        dp_psd_destroy (h);
      }
    DP_CHECK (dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 65, 0, 0.1) == NULL);
    DP_CHECK (dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, (size_t)1 << 31, 0, 0.1)
              == NULL);
    dp_psd_state_t *k22 = dp_psd_create (N, 1.0, 1, 2.2e5f, 1, 1.0, 0, 0, 0.1);
    dp_psd_state_t *b64 = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 64, 0, 0.1);
    DP_CHECK (k22 != NULL && isfinite (k22->cg));
    DP_CHECK (b64 != NULL && b64->full_scale == ldexp (1.0, 63));
    dp_psd_destroy (k22);
    dp_psd_destroy (b64);
  }

  /* ── occupied_bw outside (0, 1) is NaN (#1911 (c), (h)) ─────────────────
   * It clamped: 0 and -1 read one bin, 1.5 the full span.  NaN is the
   * convention #1901 set for an argument outside a function's domain.  1 is
   * outside too: at f = 1 the band chased float residue, so a one-bin tone
   * read 13 or 53 bins by position (h).  The rule is dp_obw_from_power's,
   * pinned in test_spectral_core.c; this pins that PSD reaches it. */
  {
    dp_psd_state_t *w = dp_psd_create (N, 1.0, 3, 0.0f, 1, 1.0, 0, 0, 0.1);
    DP_REQUIRE (w != NULL);
    const double out_of_domain[] = { 0.0, -1.0, 1.0, 1.5, NAN };
    const size_t n_ood = sizeof out_of_domain / sizeof out_of_domain[0];
    /* Before any frame too: the domain is checked before the data, and a
     * good fraction there still reads 0. */
    for (size_t i = 0; i < n_ood; i++)
      DP_CHECK (isnan (dp_psd_occupied_bw (w, out_of_domain[i])));
    DP_CHECK (dp_psd_occupied_bw (w, 0.99) == 0.0);
    float _Complex x[64];
    fill_tone (x, N, 4);
    dp_psd_accumulate (w, x, N);
    for (size_t i = 0; i < n_ood; i++)
      DP_CHECK (isnan (dp_psd_occupied_bw (w, out_of_domain[i])));
    /* precondition: inside the domain, right up to its edge, a one-bin tone
     * reads one bin */
    DP_CHECK (dp_psd_occupied_bw (w, 0.99) == 1.0 / N);
    DP_CHECK (dp_psd_occupied_bw (w, 0.999999) == 1.0 / N);
    dp_psd_destroy (w);

    /* An edge exactly on a bin boundary resolves exactly.  An impulse at
     * sample 0 has the same power, w[0]^2, in every bin, so at f = 0.5 over
     * nfft = 256 bins the excluded quarter at each end is exactly 64 bins:
     * the walk reaches it at bin 63 and the upper three quarters at bin 191,
     * 129 bins inclusive (fs = 256, so one bin is 1 Hz).  Equal bins sum
     * exactly, in any order, so this holds under -ffast-math too.
     *
     * Dividing each bin by cg^2 first, as the private copy did, rounds the
     * sums off the tie.  Where it lands depends on the build, because
     * -ffast-math may turn the division into a reciprocal multiply: rect at
     * n = 100 reads 130 only at -O3, while Kaiser at n = 64 reads 128 at
     * beta 6 and 130 at beta 1 at -O0, -O2 and -O3 alike -- measured with
     * that division put back.  All three are pinned. */
    struct
    {
      size_t n, pad;
      int    window;
      float  beta;
    } const ties[]
        = { { 100, 2, 3, 0.0f }, { 64, 4, 1, 6.0f }, { 64, 4, 1, 1.0f } };
    for (size_t k = 0; k < sizeof ties / sizeof ties[0]; k++)
      {
        dp_psd_state_t *r
            = dp_psd_create (ties[k].n, 256.0, ties[k].window, ties[k].beta,
                             ties[k].pad, 1.0, 0, 0, 0.1);
        DP_REQUIRE (r != NULL && r->nfft == 256);
        float _Complex imp[100] = { 0 };
        imp[0]                  = 1.0f;
        dp_psd_accumulate (r, imp, ties[k].n);
        DP_CHECK (dp_psd_occupied_bw (r, 0.5) == 129.0);
        dp_psd_destroy (r);
      }
  }

  /* ── DC tone lands at the centre bin after fftshift ─────────────────── */
  {
    dp_psd_state_t *w = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    float _Complex x[64];
    for (size_t i = 0; i < N; i++)
      x[i] = 1.0f + 0.0f * I;
    dp_psd_accumulate (w, x, N);
    float db[64];
    DP_CHECK (dp_psd_psd_db (w, N, db, N) == N);
    DP_CHECK (argmax (db, N) == N / 2); /* DC at index n/2 */
    dp_psd_destroy (w);
  }

  /* ── tone at bin k maps to index n/2 + k; counts frames ─────────────── */
  {
    dp_psd_state_t *w = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    const int       k = 8;
    float _Complex x[64];
    fill_tone (x, N, k);
    /* feed 3 full frames + a trailing partial that must be ignored */
    float _Complex buf[64 * 3 + 7];
    for (size_t f = 0; f < 3; f++)
      for (size_t i = 0; i < N; i++)
        buf[f * N + i] = x[i];
    for (size_t i = 0; i < 7; i++)
      buf[3 * N + i] = x[i];
    dp_psd_accumulate (w, buf, 3 * N + 7);
    DP_CHECK (w->avg->count == 3);
    float db[64];
    dp_psd_psd_db (w, N, db, N);
    DP_CHECK (argmax (db, N) == N / 2 + (size_t)k);
    dp_psd_destroy (w);
  }

  /* ── psd_dbhz differs from psd_db by a constant offset ──────────────── */
  {
    dp_psd_state_t *w = dp_psd_create (32, 2.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    float _Complex x[32];
    fill_tone (x, 32, 5);
    dp_psd_accumulate (w, x, 32);
    float a[32], b[32];
    dp_psd_psd_db (w, 32, a, 32);
    dp_psd_psd_dbhz (w, 32, b, 32);
    float off0 = a[0] - b[0];
    for (size_t i = 0; i < 32; i++)
      DP_CHECK (fabsf ((a[i] - b[i]) - off0) < 1e-3f);
    dp_psd_destroy (w);
  }

  /* ── band power: a partition sums (in power) to the total ───────────── */
  {
    dp_psd_state_t *w = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    float _Complex x[64];
    fill_tone (x, N, 10);
    for (int r = 0; r < 4; r++)
      dp_psd_accumulate (w, x, N);

    /* whole span split into two halves */
    double bands[4] = { -0.5, 0.0, 0.0, 0.5 };
    float  per[2];
    size_t nb = dp_psd_band_power (w, bands, 4, per, 2); /* per[2] */
    DP_CHECK (nb == 2);
    double total = dp_psd_total_band_power (w, bands, 4);
    /* total power = sum of the two halves' linear powers */
    double lin = pow (10.0, per[0] / 10.0) + pow (10.0, per[1] / 10.0);
    DP_CHECK (fabs (10.0 * log10 (lin) - total) < 1e-2);

    /* a band entirely outside the span integrates to the floor */
    double far[2] = { 10.0, 11.0 };
    float  pf[1];
    dp_psd_band_power (w, far, 2, pf, 1); /* pf[1] */
    DP_CHECK (pf[0] < -150.0f);
    dp_psd_destroy (w);
  }

  /* ── band power is ABSOLUTE: window- and pad-invariant ──────────────────
   * Regression for the ENBW bug (band power normalised by coherent gain cg^2
   * instead of the noise-power gain nfft*s2): a full-scale (A=1, full_scale=1)
   * tone integrates to its true power, 0 dBFS, for EVERY window and pad.  The
   * bug scaled this by 10*log10(enbw*nfft/n) — +1.76 dB (Hann), +3 dB (BH),
   * more when padded — so the reading tracked the window instead of the tone.
   */
  {
    const double whole[2] = { -0.5, 0.5 };
    /* window: 0=hann, 1=kaiser, 2=blackman-harris; pad in {1,4}. */
    for (int win = 0; win <= 2; win++)
      for (size_t pad = 1; pad <= 4; pad *= 4)
        {
          dp_psd_state_t *w
              = dp_psd_create (N, 1.0, win, 8.0f, pad, 1.0, 0, 0, 0.1);
          DP_CHECK (w != NULL);
          float _Complex x[64];
          fill_tone (x, N, 9); /* window spreads it; Parseval recovers total */
          for (int r = 0; r < 8; r++)
            dp_psd_accumulate (w, x, N);
          double p = dp_psd_total_band_power (w, whole, 2);
          DP_CHECK (fabs (p) < 0.3); /* 0 dBFS +/- 0.3 dB, any window/pad */
          dp_psd_destroy (w);
        }
  }

  /* ── occupied bandwidth: narrow for a tone, ~full for flat noise ────── */
  {
    dp_psd_state_t *w = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    float _Complex x[64];
    fill_tone (x, N, 4);
    for (int r = 0; r < 4; r++)
      dp_psd_accumulate (w, x, N);
    double obw = dp_psd_occupied_bw (w, 0.99);
    DP_CHECK (obw > 0.0 && obw < 0.5); /* a tone occupies a small fraction */
    dp_psd_destroy (w);
  }

  /* ── noise floor / SNR / SFDR are finite on a two-tone signal ───────── */
  {
    dp_psd_state_t *w = dp_psd_create (N, 1.0, 1, 8.0f, 1, 1.0, 0, 0, 0.1);
    float _Complex x[64];
    for (size_t i = 0; i < N; i++)
      {
        double p1 = 2.0 * M_PI * 6.0 * (double)i / (double)N;
        double p2 = 2.0 * M_PI * 20.0 * (double)i / (double)N;
        x[i]      = (float _Complex) ((cos (p1) + sin (p1) * I)
                                      + 0.1 * (cos (p2) + sin (p2) * I));
      }
    for (int r = 0; r < 8; r++)
      dp_psd_accumulate (w, x, N);

    double nf  = dp_psd_noise_floor (w);
    double snr = dp_psd_snr (w, 0.0, 0.2); /* band around bin 6 */
    double sf  = dp_psd_sfdr (w, -120.0f);
    DP_CHECK (isfinite (nf));
    DP_CHECK (isfinite (snr) && snr > 0.0); /* carrier above the floor    */
    DP_CHECK (isfinite (sf) && sf > 0.0);   /* carrier above the spur     */

    /* reset clears the average */
    dp_psd_reset (w);
    DP_CHECK (w->avg->count == 0);
    float db[64];
    DP_CHECK (dp_psd_psd_db (w, N, db, N) == 0);
    dp_psd_destroy (w);
  }

  /* ── cg^2 normalisation: a constant of amplitude A reads A^2 at DC ──── */
  {
    const double A = 3.0;
    float        x[64];
    for (size_t i = 0; i < N; i++)
      x[i] = (float)A;
    /* exact and pad-invariant: X[DC] = A*sum(w), so |X[DC]|^2/cg^2 = A^2 */
    for (size_t pad = 1; pad <= 2; pad++)
      {
        dp_psd_state_t *w
            = dp_psd_create (N, 1.0, 0, 0.0f, pad, 1.0, 0, 0, 0.1);
        dp_psd_accumulate_real (w, x, N);
        float  two[128];
        size_t nfft = dp_psd_power_twosided (w, w->nfft, two, w->nfft);
        DP_CHECK (nfft == w->nfft);
        DP_CHECK (fabs (two[w->nfft / 2] - A * A) < 1e-3); /* DC bin */
        float one[65];
        dp_psd_power_onesided (w, w->nfft / 2 + 1, one, w->nfft / 2 + 1);
        DP_CHECK (fabs (one[0] - A * A) < 1e-3); /* one-sided DC */
        dp_psd_destroy (w);
      }
  }

  /* ── ENBW: Hann ~1.5 bins; Kaiser(beta=8) wider ─────────────────────── */
  {
    dp_psd_state_t *h = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    dp_psd_state_t *k = dp_psd_create (N, 1.0, 1, 8.0f, 1, 1.0, 0, 0, 0.1);
    DP_CHECK (fabs (h->enbw - 1.5) < 0.05);
    DP_CHECK (k->enbw
              > h->enbw); /* a Kaiser(8) main lobe is wider than Hann */
    dp_psd_destroy (h);
    dp_psd_destroy (k);
  }

  /* ── one-sided fold conserves energy: sum(one) == sum(two) ──────────── */
  {
    dp_psd_state_t *w = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    float           x[64];
    fill_real_tone (x, N, 7, 0.5);
    dp_psd_accumulate_real (w, x, N);
    float two[64], one[33];
    dp_psd_power_twosided (w, N, two, N);
    size_t no = dp_psd_power_onesided (w, N / 2 + 1, one, N / 2 + 1);
    DP_CHECK (no == N / 2 + 1);
    double st = 0.0, so = 0.0;
    for (size_t i = 0; i < N; i++)
      st += two[i];
    for (size_t i = 0; i < no; i++)
      so += one[i];
    DP_CHECK (fabs (st - so) < 1e-4 * st);
    dp_psd_destroy (w);
  }

  /* ── zero-padding scales total power by nfft/n (the cal cores correct) ─ */
  {
    float x[64];
    fill_real_tone (x, N, 9, 0.7);
    double tot[3] = { 0 };
    for (size_t pad = 1; pad <= 2; pad++)
      {
        dp_psd_state_t *w
            = dp_psd_create (N, 1.0, 0, 0.0f, pad, 1.0, 0, 0, 0.1);
        dp_psd_accumulate_real (w, x, N);
        float  two[128];
        size_t nfft = dp_psd_power_twosided (w, w->nfft, two, w->nfft);
        for (size_t i = 0; i < nfft; i++)
          tot[pad] += two[i];
        dp_psd_destroy (w);
      }
    /* nfft doubles from pad=1 (64) to pad=2 (128): total scales ~2x */
    DP_CHECK (fabs (tot[2] / tot[1] - 2.0) < 0.05);
  }

  /* ── mean of K identical frames equals a single frame (exact) ───────── */
  {
    float x[64];
    fill_real_tone (x, N, 11, 0.4);
    dp_psd_state_t *w = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    dp_psd_accumulate_real (w, x, N);
    float a[33];
    dp_psd_power_onesided (w, N / 2 + 1, a, N / 2 + 1);
    dp_psd_reset (w);
    float buf[64 * 5];
    for (size_t f = 0; f < 5; f++)
      for (size_t i = 0; i < N; i++)
        buf[f * N + i] = x[i];
    dp_psd_accumulate_real (w, buf, 5 * N);
    float b[33];
    dp_psd_power_onesided (w, N / 2 + 1, b, N / 2 + 1);
    for (size_t i = 0; i < N / 2 + 1; i++)
      DP_CHECK (fabsf (a[i] - b[i]) < 1e-6f * (fabsf (a[i]) + 1e-6f));
    dp_psd_destroy (w);
  }

  /* ── maxhold >= minhold per bin over differing frames ───────────────── */
  {
    float f1[64], f2[64];
    fill_real_tone (f1, N, 8, 1.0);
    fill_real_tone (f2, N, 16, 1.0);
    float buf[128];
    for (size_t i = 0; i < N; i++)
      buf[i] = f1[i];
    for (size_t i = 0; i < N; i++)
      buf[N + i] = f2[i];

    dp_psd_state_t *mx
        = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, ACC_TRACE_MAXHOLD, 0.1);
    dp_psd_state_t *mn
        = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, ACC_TRACE_MINHOLD, 0.1);
    dp_psd_accumulate_real (mx, buf, 2 * N);
    dp_psd_accumulate_real (mn, buf, 2 * N);
    float pmx[64], pmn[64];
    dp_psd_power_twosided (mx, N, pmx, N);
    dp_psd_power_twosided (mn, N, pmn, N);
    for (size_t i = 0; i < N; i++)
      DP_CHECK (pmx[i] >= pmn[i] - 1e-6f);
    dp_psd_destroy (mx);
    dp_psd_destroy (mn);
  }

  /* ── averaging tightens the noise-floor estimate (deterministic) ────── */
  {
    const size_t K   = 64;
    uint64_t     rng = 0x1234567890abcdefULL;
    float       *buf = (float *)malloc (K * N * sizeof (float));
    for (size_t i = 0; i < K * N; i++)
      buf[i] = lcg_unit (&rng);

    dp_psd_state_t *w1 = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    dp_psd_state_t *wK = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    dp_psd_accumulate_real (w1, buf, N);     /* 1 frame  */
    dp_psd_accumulate_real (wK, buf, K * N); /* K frames */
    float p1[64], pK[64];
    dp_psd_power_twosided (w1, N, p1, N);
    dp_psd_power_twosided (wK, N, pK, N);
    /* white noise: the K-averaged spectrum is flatter (smaller spread). */
    DP_CHECK (stddev (pK, 1, N) < stddev (p1, 1, N));
    free (buf);
    dp_psd_destroy (w1);
    dp_psd_destroy (wK);
  }

  /* ── pass_capacity: emission stops at max_out (jm gh-138) ────────── */
  {
    /* Every readout here used to ignore its count argument entirely and
     * write state->nfft floats regardless of what the caller owned. */
    const size_t    N = 64;
    dp_psd_state_t *w = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
    float _Complex x[64];
    float buf[64];
    DP_CHECK (w != NULL);
    fill_tone (x, N, 10);
    dp_psd_accumulate (w, x, N);

    for (size_t i = 0; i < N; i++)
      buf[i] = 42.0f;
    DP_CHECK (dp_psd_power_twosided (w, N, buf, 5) == 5);
    for (size_t i = 5; i < N; i++)
      DP_CHECK (buf[i] == 42.0f); /* tail untouched */

    for (size_t i = 0; i < N; i++)
      buf[i] = 42.0f;
    DP_CHECK (dp_psd_psd_db (w, N, buf, 3) == 3);
    for (size_t i = 3; i < N; i++)
      DP_CHECK (buf[i] == 42.0f);

    for (size_t i = 0; i < N; i++)
      buf[i] = 42.0f;
    DP_CHECK (dp_psd_psd_dbhz (w, N, buf, 3) == 3);
    for (size_t i = 3; i < N; i++)
      DP_CHECK (buf[i] == 42.0f);

    /* power_onesided writes out[0] and out[half] OUTSIDE its loop, so a
     * loop-only clamp would still scribble at index half (== 32 here). */
    for (size_t i = 0; i < N; i++)
      buf[i] = 42.0f;
    DP_CHECK (dp_psd_power_onesided (w, N / 2 + 1, buf, 4) == 4);
    for (size_t i = 4; i < N; i++)
      DP_CHECK (buf[i] == 42.0f); /* in particular buf[32], the half index */

    /* Zero capacity emits nothing from any of them. */
    for (size_t i = 0; i < N; i++)
      buf[i] = 42.0f;
    DP_CHECK (dp_psd_power_twosided (w, N, buf, 0) == 0);
    DP_CHECK (dp_psd_power_onesided (w, N / 2 + 1, buf, 0) == 0);
    DP_CHECK (dp_psd_psd_db (w, N, buf, 0) == 0);
    for (size_t i = 0; i < N; i++)
      DP_CHECK (buf[i] == 42.0f);

    /* band_power emits one value per BAND -- half the edge count. */
    double bands[4] = { -0.5, 0.0, 0.0, 0.5 };
    float  per[2]   = { 42.0f, 42.0f };
    DP_CHECK (dp_psd_band_power (w, bands, 4, per, 1) == 1);
    DP_CHECK (per[1] == 42.0f);
    dp_psd_destroy (w);
  }

  /* serializable state — delegates to the acc_trace averager child. */
  {
    float _Complex frame[64];
    for (int i = 0; i < 64; i++)
      frame[i] = (float)(i % 8) - 4.0f + 0.3f * I;
    dp_psd_state_t *a = dp_psd_create (64, 1.0e6, 1, 8.0f, 1, 1.0, 0, 0, 0.1);
    dp_psd_state_t *b = dp_psd_create (64, 1.0e6, 1, 8.0f, 1, 1.0, 0, 0, 0.1);
    DP_CHECK (a != NULL && b != NULL);
    dp_psd_accumulate (a, frame, 64);
    dp_psd_accumulate (a, frame, 64);
    DP_STATE_ROUNDTRIP_TEST (dp_psd, a, b);
    DP_CHECK (b->avg->count == a->avg->count);
    dp_psd_destroy (a);
    dp_psd_destroy (b);
  }

  /* ── the per-frame kernel: PSD with the average taken out ───────────────
   * dp_psd_frame_power / dp_psd_frame_db run the same code dp_psd_accumulate
   * folds, so for ONE frame they must equal what accumulate-then-read gives,
   * bit for bit, for every window and with zero-padding. A spectrogram row
   * built on a second kernel (FFT + dp_magnitude_db_cf32) would differ from
   * the PSD of the same frame by the window's coherent gain. */
  {
    static const size_t ns[]   = { 64, 100 }; /* 100: not a power of two */
    static const size_t pads[] = { 1, 2 };
    uint32_t            seed   = 12345u;
    for (int win = 0; win <= 3; win++)
      for (size_t a = 0; a < 2; a++)
        for (size_t b = 0; b < 2; b++)
          {
            const size_t n = ns[a];
            float _Complex x[100];
            for (size_t i = 0; i < n; i++)
              {
                /* named locals: two draws in one expression have no order */
                const float re = (float)(dp_uni (&seed) - 0.5);
                const float im = (float)(dp_uni (&seed) - 0.5);
                x[i]           = CMPLXF (re, im);
              }

            dp_psd_state_t *ref
                = dp_psd_create (n, 1.0, win, 6.0f, pads[b], 1.0, 0, 0, 0.0);
            dp_psd_state_t *k
                = dp_psd_create (n, 1.0, win, 6.0f, pads[b], 1.0, 0, 0, 0.0);
            DP_REQUIRE (ref && k);
            const size_t nfft    = ref->nfft;
            float       *want_db = malloc (nfft * sizeof *want_db);
            float       *want_p  = malloc (nfft * sizeof *want_p);
            float       *got_db  = malloc (nfft * sizeof *got_db);
            float       *got_p   = malloc (nfft * sizeof *got_p);
            DP_REQUIRE (want_db && want_p && got_db && got_p);

            dp_psd_accumulate (ref, x, n);
            DP_CHECK (dp_psd_psd_db (ref, nfft, want_db, nfft) == nfft);
            DP_CHECK (dp_psd_power_twosided (ref, nfft, want_p, nfft) == nfft);

            /* The kernel leaves the average alone: nothing accumulated. */
            dp_psd_frame_db (k, x, got_db);
            DP_CHECK (dp_psd_psd_db (k, nfft, got_p, nfft) == 0);
            DP_CHECK (memcmp (got_db, want_db, nfft * sizeof *got_db) == 0);

            /* raw power / cg^2 is exactly the two-sided readout */
            dp_psd_frame_power (k, x, got_p);
            const double cg2  = ref->cg * ref->cg;
            int          same = 1;
            for (size_t i = 0; i < nfft; i++)
              if ((float)((double)got_p[i] / cg2) != want_p[i])
                same = 0;
            DP_CHECK (same);

            free (want_db);
            free (want_p);
            free (got_db);
            free (got_p);
            dp_psd_destroy (ref);
            dp_psd_destroy (k);
          }
  }

  /* ── rectangular window (index 3): no taper, and the index is bounded ─── */
  {
    enum
    {
      RN = 32
    };
    dp_psd_state_t *r = dp_psd_create (RN, 1.0, 3, 0.0f, 1, 1.0, 0, 0, 0.0);
    DP_REQUIRE (r != NULL);
    int all_ones = 1;
    for (size_t i = 0; i < RN; i++)
      if (r->w[i] != 1.0f)
        all_ones = 0;
    DP_CHECK (all_ones);
    DP_CHECK (r->cg == (double)RN); /* sum of ones */
    float _Complex x[RN];
    float db[RN];
    fill_tone (x, RN, 5);
    dp_psd_frame_db (r, x, db);
    /* a unit tone on a bin reads 0 dBFS: cg^2 normalisation is window-blind */
    DP_CHECK (fabsf (db[RN / 2 + 5]) < 1e-3f);
    dp_psd_destroy (r);
    DP_CHECK (dp_psd_create (RN, 1.0, 4, 0.0f, 1, 1.0, 0, 0, 0.0) == NULL);
    DP_CHECK (dp_psd_create (RN, 1.0, -1, 0.0f, 1, 1.0, 0, 0, 0.0) == NULL);
  }

  DP_TEST_END ("test_psd_core");
}
