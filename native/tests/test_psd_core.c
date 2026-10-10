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

/* n samples of complex white Gaussian noise with E|x|^2 = var, from the one
 * harness generator (dp_rng_test.h). */
static void
fill_cnoise (float _Complex *x, size_t n, double var, uint32_t *seed)
{
  const float s = (float)sqrt (var);
  for (size_t i = 0; i < n; i++)
    x[i] = s * dp_cgauss (seed);
}

/* sum(w^4) / sum(w^2)^2 for the state's window.  For complex white noise a
 * frame's windowed energy sum |w_i x_i|^2 has mean var*s2 and, |x|^2 being
 * Exp(var), variance var^2 * sum w^4: so this is the squared relative
 * spread of one frame's energy, and /K that of a K-frame average. */
static double
w4_ratio (const dp_psd_state_t *s)
{
  double s4 = 0.0;
  for (size_t i = 0; i < s->n; i++)
    {
      const double w2 = (double)s->w[i] * (double)s->w[i];
      s4 += w2 * w2;
    }
  return s4 / (s->s2 * s->s2);
}

/* Mean over the first n entries, in double. */
static double
mean_f (const float *a, size_t n)
{
  double m = 0.0;
  for (size_t i = 0; i < n; i++)
    m += a[i];
  return m / (double)n;
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
    float  pf[1]  = { NAN };
    DP_CHECK (dp_psd_band_power (w, far, 2, pf, 1) == 1);
    DP_CHECK (pf[0] == -200.0f); /* 10 log10 of PSD's 1e-20 floor */
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

  /* ════════════════════════════════════════════════════════════════════════
   * The rest of PSD's header, certified (#1911, part b).  The claims the
   * inventory found absent or pinned only at literals: the empty contract
   * (T6), linear power without full_scale (T7), real input (T8), the four
   * averaging modes against an external truth (T9), reset (T10), dB/Hz as an
   * absolute (T11), band power (T12), occupied bandwidth (T13), the noise
   * floor (T14), SNR (T15), SFDR (T16) and create's remaining arguments (T17).
   *
   * Statistical tests draw from dp_rng_test.h and size their tolerance from
   * the estimator's own spread at z = 5: a correct estimator fails on fewer
   * than 1 seed in 10^6, so a red here is the code, not the draw.
   * ════════════════════════════════════════════════════════════════════════
   */

  /* T6: every reader's empty contract.  Before any frame, and again after a
   * reset, the readouts return 0 and write nothing, total band power reads
   * the -200 dB floor and the scalars return 0.  Only psd_db's was pinned.
   * The post-reset pass carries its precondition: the same readers were
   * non-empty the moment before. */
  {
    dp_psd_state_t *w = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.0);
    DP_REQUIRE (w != NULL);
    float _Complex x[64];
    fill_tone (x, N, 6);
    const double bands[2] = { -0.25, 0.25 };
    for (int pass = 0; pass < 2; pass++)
      {
        float buf[64];
        if (pass == 1)
          {
            dp_psd_accumulate (w, x, N);
            DP_REQUIRE (dp_psd_power_twosided (w, N, buf, N) == N);
            DP_REQUIRE (dp_psd_occupied_bw (w, 0.99) > 0.0);
            DP_REQUIRE (dp_psd_snr (w, -0.25, 0.25) != 0.0);
            dp_psd_reset (w);
          }
        for (size_t i = 0; i < N; i++)
          buf[i] = 42.0f;
        DP_CHECK (dp_psd_power_twosided (w, N, buf, N) == 0);
        DP_CHECK (dp_psd_power_onesided (w, N / 2 + 1, buf, N / 2 + 1) == 0);
        DP_CHECK (dp_psd_psd_dbhz (w, N, buf, N) == 0);
        DP_CHECK (dp_psd_band_power (w, bands, 2, buf, 1) == 0);
        int untouched = 1;
        for (size_t i = 0; i < N; i++)
          if (buf[i] != 42.0f)
            untouched = 0;
        DP_CHECK (untouched);
        DP_CHECK (fabs (dp_psd_total_band_power (w, bands, 2) + 200.0) < 1e-4);
        DP_CHECK (dp_psd_occupied_bw (w, 0.99) == 0.0);
        DP_CHECK (dp_psd_noise_floor (w) == 0.0);
        DP_CHECK (dp_psd_snr (w, -0.25, 0.25) == 0.0);
        DP_CHECK (dp_psd_sfdr (w, -120.0f) == 0.0);
      }
    dp_psd_destroy (w);
  }

  /* T7: the linear-power readouts do NOT apply full_scale, the dB ones do.
   * The same frame into full_scale = 1, full_scale = 4 and bits = 3 (whose
   * reference is also 4): the two- and one-sided powers are bit-identical,
   * and -- the precondition that full_scale is live at all -- psd_db moves
   * by exactly 20*log10(4) = 12.04 dB.  Tolerance on that: dB values here
   * are under 100 in magnitude, where a float's spacing is <= 7.6e-6. */
  {
    uint32_t seed = 19112u;
    float _Complex x[64];
    fill_cnoise (x, N, 1.0, &seed);
    dp_psd_state_t *w1 = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.0);
    dp_psd_state_t *w4 = dp_psd_create (N, 1.0, 0, 0.0f, 1, 4.0, 0, 0, 0.0);
    dp_psd_state_t *wb = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 3, 0, 0.0);
    DP_REQUIRE (w1 && w4 && wb);
    dp_psd_accumulate (w1, x, N);
    dp_psd_accumulate (w4, x, N);
    dp_psd_accumulate (wb, x, N);
    float a[64], b[64], c[64];
    dp_psd_power_twosided (w1, N, a, N);
    dp_psd_power_twosided (w4, N, b, N);
    dp_psd_power_twosided (wb, N, c, N);
    DP_CHECK (memcmp (a, b, sizeof a) == 0);
    DP_CHECK (memcmp (a, c, sizeof a) == 0);
    dp_psd_power_onesided (w1, N / 2 + 1, a, N / 2 + 1);
    dp_psd_power_onesided (w4, N / 2 + 1, b, N / 2 + 1);
    DP_CHECK (memcmp (a, b, (N / 2 + 1) * sizeof a[0]) == 0);
    dp_psd_psd_db (w1, N, a, N);
    dp_psd_psd_db (w4, N, b, N);
    const double step  = 20.0 * log10 (4.0);
    int          moved = 1;
    for (size_t i = 0; i < N; i++)
      if (fabs (((double)a[i] - (double)b[i]) - step) > 1e-4)
        moved = 0;
    DP_CHECK (moved);
    dp_psd_destroy (w1);
    dp_psd_destroy (w4);
    dp_psd_destroy (wb);
  }

  /* T8: real input.  A real frame is Hermitian, so +k and -k carry equal
   * power; the one-sided fold keeps DC and Nyquist as-is and sums the two
   * halves of every interior bin; and floor(x_len / n) frames are taken.
   *
   * Tolerance for the symmetry: +k and -k leave the complex FFT by different
   * arithmetic.  A radix-2 FFT's error obeys ||dX|| <= c log2(nfft) u ||X||
   * (Higham, Accuracy and Stability of Numerical Algorithms, sec. 24.1;
   * c ~ 5, u = 2^-24), so one bin's power moves by at most
   * 2 |X_k| |dX| + |dX|^2 <= ~2 c log2(nfft) u * sum|X|^2: 3.6e-6 of the
   * total at nfft = 64.  A broken symmetry (a dropped conjugate, a fold
   * reading the wrong half) is O(1) of a bin. */
  {
    uint32_t seed = 19113u;
    float    x[64 * 2 + 7];
    for (size_t i = 0; i < sizeof x / sizeof x[0]; i++)
      x[i] = (float)dp_gauss (&seed);
    dp_psd_state_t *w = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.0);
    DP_REQUIRE (w != NULL);
    dp_psd_accumulate_real (w, x, sizeof x / sizeof x[0]);
    DP_CHECK (w->avg->count == 2); /* the 7-sample tail is ignored */
    float two[64], one[33];
    DP_REQUIRE (dp_psd_power_twosided (w, N, two, N) == N);
    DP_REQUIRE (dp_psd_power_onesided (w, N / 2 + 1, one, N / 2 + 1)
                == N / 2 + 1);
    const size_t h    = N / 2;
    const double tot  = mean_f (two, N) * (double)N;
    const double tol  = 2.0 * 5.0 * log2 ((double)N) * ldexp (1.0, -24) * tot;
    int          herm = 1, fold = 1;
    for (size_t m = 1; m < h; m++)
      {
        if (fabs ((double)two[h + m] - (double)two[h - m]) > tol)
          herm = 0;
        /* one[m] and the two halves each went through one float rounding */
        const double halves = (double)two[h + m] + (double)two[h - m];
        if (fabs ((double)one[m] - halves) > 4.0 * ldexp (1.0, -24) * halves)
          fold = 0;
      }
    DP_CHECK (herm);
    DP_CHECK (fold);
    DP_CHECK (one[0] == two[h]); /* DC: no mirror partner, kept as-is */
    DP_CHECK (one[h] == two[0]); /* Nyquist: likewise */
    dp_psd_destroy (w);

    /* The fold again on a COMPLEX frame, where +k and -k carry different
     * power.  On a real frame the halves are equal, so a fold that doubled
     * +k instead of adding -k passes the check above (found by sabotage).
     * Precondition: the halves do differ, by more than 10% somewhere. */
    float _Complex z[64];
    fill_cnoise (z, N, 1.0, &seed);
    dp_psd_state_t *c = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.0);
    DP_REQUIRE (c != NULL);
    dp_psd_accumulate (c, z, N);
    DP_REQUIRE (dp_psd_power_twosided (c, N, two, N) == N);
    DP_REQUIRE (dp_psd_power_onesided (c, N / 2 + 1, one, N / 2 + 1)
                == N / 2 + 1);
    int asym = 0;
    fold     = 1;
    for (size_t m = 1; m < h; m++)
      {
        const double halves = (double)two[h + m] + (double)two[h - m];
        if (fabs ((double)two[h + m] - (double)two[h - m]) > 0.1 * halves)
          asym = 1;
        if (fabs ((double)one[m] - halves) > 4.0 * ldexp (1.0, -24) * halves)
          fold = 0;
      }
    DP_REQUIRE (asym);
    DP_CHECK (fold);
    dp_psd_destroy (c);
  }

  /* T9: the four averaging modes, each against a truth built from the
   * kernel's own single-frame powers rather than from another mode.  The
   * old check (max >= min per bin) passed with two MEAN states.  Four
   * distinct frames at 0, +9.5, -6 and +6 dB, so every mode gives a
   * different answer.
   *   mean:    the per-bin arithmetic mean
   *   exp:     y1 = P1, then y += alpha (P - y)   (alpha = 0.25)
   *   maxhold: the per-bin maximum -- exact, a float copied and returned
   *   minhold: the per-bin minimum -- exact
   * mean and exp are computed in double here and by Welford / the EMA step
   * there; both then pass two float roundings (the trace value and the
   * /cg^2), so they agree to 4 u relative.  The exp mode is deterministic:
   * its claim is the recursion, which noise would only re-derive. */
  {
    static const float scales[4] = { 1.0f, 3.0f, 0.5f, 2.0f };
    const double       alpha     = 0.25;
    uint32_t           seed      = 19114u;
    float _Complex f[4][64];
    float           P[4][64];
    dp_psd_state_t *k = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.0);
    DP_REQUIRE (k != NULL);
    for (size_t j = 0; j < 4; j++)
      {
        fill_cnoise (f[j], N, 1.0, &seed);
        for (size_t i = 0; i < N; i++)
          f[j][i] *= scales[j];
        dp_psd_frame_power (k, f[j], P[j]);
      }
    const double cg2 = k->cg * k->cg;
    for (int mode = 0; mode <= 3; mode++)
      {
        dp_psd_state_t *w
            = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, mode, alpha);
        DP_REQUIRE (w != NULL);
        for (size_t j = 0; j < 4; j++)
          dp_psd_accumulate (w, f[j], N);
        float got[64];
        DP_REQUIRE (dp_psd_power_twosided (w, N, got, N) == N);
        int ok = 1;
        for (size_t i = 0; i < N; i++)
          {
            double y = P[0][i];
            for (size_t j = 1; j < 4; j++)
              {
                const double p = P[j][i];
                if (mode == ACC_TRACE_MEAN)
                  y += p;
                else if (mode == ACC_TRACE_EXP)
                  y += alpha * (p - y);
                else if (mode == ACC_TRACE_MAXHOLD)
                  y = p > y ? p : y;
                else
                  y = p < y ? p : y;
              }
            if (mode == ACC_TRACE_MEAN)
              y /= 4.0;
            const double want = (double)(float)((double)(float)y / cg2);
            if (mode == ACC_TRACE_MAXHOLD || mode == ACC_TRACE_MINHOLD)
              {
                if ((double)got[i] != want)
                  ok = 0;
              }
            else if (fabs ((double)got[i] - want)
                     > 4.0 * ldexp (1.0, -24) * want)
              ok = 0;
          }
        DP_CHECK (ok); /* this mode matches its defining rule */
        dp_psd_destroy (w);
      }
    dp_psd_destroy (k);
  }

  /* T10: reset re-seeds.  The old check reset and re-fed IDENTICAL frames
   * in MEAN mode, where Welford's count = 1 step re-seeds whatever reset
   * did.  Here, in maxhold and in exp -- the modes that remember -- a loud
   * frame A, a reset, then a quiet frame B must read exactly what a fresh
   * state reads after B alone.  Precondition: before the reset the loud
   * frame does show. */
  {
    uint32_t seed = 19115u;
    float _Complex a[64], b[64];
    fill_cnoise (a, N, 100.0, &seed); /* +20 dB */
    fill_cnoise (b, N, 1.0, &seed);
    static const int modes[2] = { ACC_TRACE_MAXHOLD, ACC_TRACE_EXP };
    for (size_t j = 0; j < 2; j++)
      {
        dp_psd_state_t *fresh
            = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, modes[j], 0.25);
        dp_psd_state_t *w
            = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, modes[j], 0.25);
        DP_REQUIRE (fresh && w);
        float want[64], got[64];
        dp_psd_accumulate (fresh, b, N);
        dp_psd_power_twosided (fresh, N, want, N);
        dp_psd_accumulate (w, a, N);
        dp_psd_power_twosided (w, N, got, N);
        DP_REQUIRE (memcmp (got, want, sizeof got) != 0);
        dp_psd_reset (w);
        dp_psd_accumulate (w, b, N);
        DP_CHECK (w->avg->count == 1);
        dp_psd_power_twosided (w, N, got, N);
        DP_CHECK (memcmp (got, want, sizeof got) == 0);
        dp_psd_destroy (fresh);
        dp_psd_destroy (w);
      }
  }

  /* T11: psd_dbhz is an absolute density.  Complex white noise of variance
   * var, sampled at fs, has two-sided density var/fs per Hz -- padded or
   * not -- and psd_dbhz differs from psd_db by exactly
   * 10*log10(cg^2 / (fs*s2)).  The old test pinned "a constant", not which.
   *
   * Statistic: the mean over bins of the linear density.  By Parseval the
   * mean over nfft bins of |X_k|^2 is sum_i w_i^2 |x_i|^2 (zero-padding adds
   * no energy), so after K frames it estimates var*s2 with relative
   * standard deviation sigma = sqrt(w4_ratio / K) -- see w4_ratio().  Hann,
   * n = 64: sqrt(w4_ratio) = 0.174, and K = 1024 gives sigma = 0.54%.
   * Tolerance z = 5: 10*log10(1 + 5 sigma) = 0.12 dB, against the >= 1.76 dB
   * a lost or doubled ENBW would cost a Hann window. */
  {
    const size_t K   = 1024;
    const double var = 4.0, fs = 2.0; /* density 2 per Hz: +3.01 dB/Hz */
    for (size_t pad = 1; pad <= 2; pad++)
      {
        uint32_t        seed = 19116u + (uint32_t)pad;
        dp_psd_state_t *w
            = dp_psd_create (N, fs, 0, 0.0f, pad, 1.0, 0, 0, 0.0);
        DP_REQUIRE (w != NULL);
        float _Complex x[64];
        for (size_t f = 0; f < K; f++)
          {
            fill_cnoise (x, N, var, &seed);
            dp_psd_accumulate (w, x, N);
          }
        const size_t nfft = w->nfft;
        float       *hz   = malloc (nfft * sizeof *hz);
        float       *db   = malloc (nfft * sizeof *db);
        DP_REQUIRE (hz && db);
        DP_REQUIRE (dp_psd_psd_dbhz (w, nfft, hz, nfft) == nfft);
        DP_REQUIRE (dp_psd_psd_db (w, nfft, db, nfft) == nfft);
        double       lin      = 0.0;
        int          off      = 1;
        const double want_off = 10.0 * log10 (w->cg * w->cg / (fs * w->s2));
        for (size_t i = 0; i < nfft; i++)
          {
            lin += pow (10.0, hz[i] / 10.0);
            if (fabs (((double)hz[i] - (double)db[i]) - want_off) > 1e-4)
              off = 0;
          }
        lin /= (double)nfft;
        const double sigma = sqrt (w4_ratio (w) / (double)K);
        const double tol   = 10.0 * log10 (1.0 + 5.0 * sigma);
        DP_CHECK (fabs (10.0 * log10 (lin) - 10.0 * log10 (var / fs)) < tol);
        DP_CHECK (off);
        free (hz);
        free (db);
        dp_psd_destroy (w);
      }
  }

  /* T12: band power.  (a) The rectangular window, which the window/pad
   * sweep above leaves out, reads a full-scale tone at 0 dBFS -- exact by
   * Parseval; summing nfft float bins rounds by <= nfft u (1.5e-5 at
   * nfft = 256), 7e-5 dB, so 1e-3 dB.  (b) White noise integrates to its
   * variance over the span, for every window and pad: the whole-span band is
   * sum pwr / (nfft s2), T11's statistic over s2, with the same sigma and
   * the same z = 5.  (c) Edges are clamped to the span: [-fs, fs] reads the
   * same bins as [-fs/2, fs/2], bit for bit.  (d) Two halves sum to the
   * WHOLE span, an external truth -- the old partition check compared
   * total_band_power with the sum of the same per-band values.  The tone is
   * off the bin the halves share (DC); with power on it each half counts
   * it, which part c of #1911 measures as finding (e). */
  {
    const double whole[2] = { -0.5, 0.5 };
    for (size_t pad = 1; pad <= 4; pad *= 4)
      {
        dp_psd_state_t *w
            = dp_psd_create (N, 1.0, 3, 0.0f, pad, 1.0, 0, 0, 0.0);
        DP_REQUIRE (w != NULL);
        float _Complex x[64];
        fill_tone (x, N, 9);
        dp_psd_accumulate (w, x, N);
        DP_CHECK (fabs (dp_psd_total_band_power (w, whole, 2)) < 1e-3);
        dp_psd_destroy (w);
      }

    const size_t K   = 1024;
    const double var = 0.5;
    for (int win = 0; win <= 3; win++)
      for (size_t pad = 1; pad <= 2; pad++)
        {
          uint32_t        seed = 19117u + (uint32_t)(10 * win) + (uint32_t)pad;
          dp_psd_state_t *w
              = dp_psd_create (N, 1.0, win, 8.0f, pad, 1.0, 0, 0, 0.0);
          DP_REQUIRE (w != NULL);
          float _Complex x[64];
          for (size_t f = 0; f < K; f++)
            {
              fill_cnoise (x, N, var, &seed);
              dp_psd_accumulate (w, x, N);
            }
          const double sigma = sqrt (w4_ratio (w) / (double)K);
          const double tol   = 10.0 * log10 (1.0 + 5.0 * sigma);
          DP_CHECK (
              fabs (dp_psd_total_band_power (w, whole, 2) - 10.0 * log10 (var))
              < tol);
          const double wide[2] = { -1.0, 1.0 };
          DP_CHECK (dp_psd_total_band_power (w, wide, 2)
                    == dp_psd_total_band_power (w, whole, 2));
          dp_psd_destroy (w);
        }

    dp_psd_state_t *w = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.0);
    DP_REQUIRE (w != NULL);
    float _Complex x[64];
    fill_tone (x, N, 10);
    dp_psd_accumulate (w, x, N);
    const double halves[4] = { -0.5, 0.0, 0.0, 0.5 };
    float        per[2];
    DP_REQUIRE (dp_psd_band_power (w, halves, 4, per, 2) == 2);
    const double sum
        = 10.0 * log10 (pow (10.0, per[0] / 10.0) + pow (10.0, per[1] / 10.0));
    DP_CHECK (fabs (sum - dp_psd_total_band_power (w, whole, 2)) < 1e-3);
    dp_psd_destroy (w);
  }

  /* T13: occupied bandwidth.  (a) A bin-centred tone under the rectangular
   * window has all its power in one bin: OBW = fs/nfft exactly.  (b) Flat
   * noise: OBW(0.99) is 0.99 fs to within the bin grid.  For a flat
   * spectrum over nfft = 1024 bins the lower edge is the first bin where the
   * running power reaches 0.5% of the total and the upper the first reaching
   * 99.5%: bins 5 and 1018, 1014 bins.  Rectangular and unpadded, so the bins
   * are independent K-averages of Exp, relative sd 1/sqrt(K); a running sum
   * over the m ~ 5 bins at an edge has sd (m/nfft)/sqrt(mK) of the total,
   * 1.2e-4 at K = 256.  The flat crossing sits just inside each edge -- the
   * threshold is 1.2e-4 of the total past bin 4's running sum (and short of
   * bin 1018's) -- so a ~1 sigma excursion moves an edge OUT by one bin,
   * while moving it in needs 8.6e-4, ~6 sigma, and out by two, ~9 sigma.
   * So 1014 <= OBW <= 1016 bins (measured 1016 on this seed); the bound
   * asserted, [1012, 1016], is that with the 6-sigma inward move allowed.
   * (c) An all-zero input has no power: 0. */
  {
    dp_psd_state_t *t = dp_psd_create (N, 1.0, 3, 0.0f, 1, 1.0, 0, 0, 0.0);
    DP_REQUIRE (t != NULL);
    float _Complex x[64];
    fill_tone (x, N, 7);
    dp_psd_accumulate (t, x, N);
    DP_CHECK (dp_psd_occupied_bw (t, 0.99) == 1.0 / (double)N);
    dp_psd_destroy (t);

    const size_t    NN = 1024, K = 256;
    uint32_t        seed = 19118u;
    dp_psd_state_t *w    = dp_psd_create (NN, 1.0, 3, 0.0f, 1, 1.0, 0, 0, 0.0);
    float _Complex *y    = malloc (NN * sizeof *y);
    DP_REQUIRE (w && y);
    for (size_t f = 0; f < K; f++)
      {
        fill_cnoise (y, NN, 1.0, &seed);
        dp_psd_accumulate (w, y, NN);
      }
    /* fs = 1 and nfft = 1024: OBW * nfft is an exact integer bin count */
    const double bins = dp_psd_occupied_bw (w, 0.99) * (double)NN;
    DP_CHECK (bins >= 1012.0 && bins <= 1016.0);
    free (y);
    dp_psd_destroy (w);

    dp_psd_state_t *z = dp_psd_create (N, 1.0, 3, 0.0f, 1, 1.0, 0, 0, 0.0);
    DP_REQUIRE (z != NULL);
    for (size_t i = 0; i < N; i++)
      x[i] = 0.0f;
    dp_psd_accumulate (z, x, N);
    DP_CHECK (dp_psd_occupied_bw (z, 0.99) == 0.0);
    dp_psd_destroy (z);
  }

  /* T14 and T15: the noise floor is the MEDIAN of the averaged dB spectrum,
   * and SNR is the in-band peak above it.  Rectangular, unpadded, n = 256
   * (independent bins), K = 256, complex noise of variance 1 with a unit
   * bin-centred tone for SNR.
   *
   * Floor.  A bin's psd_db is 10 log10 of a K-average of Exp with mean
   * var s2 / cg^2 = var/n.  That average is Gamma(K) with median
   * 1 - 1/(3K) of its mean (Wilson-Hilferty) and relative sd 1/sqrt(K); the
   * median of nfft such bins has sd sqrt(pi/2)/sqrt(nfft K) = 0.49%.
   * Expected floor: 10 log10(var/n) + 10 log10(1 - 1/(3K)) = -24.088 dB.
   *
   * SNR.  The tone bin averages |A n + N_k|^2 / n^2 to A^2 + var/n; its
   * spread is dominated by the cross term 2 Re(A n conj N_k), relative sd
   * sqrt(2 var / n) / A per frame, 0.55% over K.  SNR = 10 log10((A^2 +
   * var/n) / (var/n)) minus the floor's median bias = 24.105 dB, with sd
   * sqrt(0.49^2 + 0.55^2) = 0.74%.
   *
   * Tolerance z = 5 on each, from those sigmas, in dB. */
  {
    const size_t    NN = 256, K = 256;
    const double    var  = 1.0;
    uint32_t        seed = 19119u;
    dp_psd_state_t *w    = dp_psd_create (NN, 1.0, 3, 0.0f, 1, 1.0, 0, 0, 0.0);
    float _Complex *y    = malloc (NN * sizeof *y);
    float _Complex *s    = malloc (NN * sizeof *s);
    DP_REQUIRE (w && y && s);
    const int kt = 40;
    fill_tone (s, NN, kt);
    for (size_t f = 0; f < K; f++)
      {
        fill_cnoise (y, NN, var, &seed);
        for (size_t i = 0; i < NN; i++)
          y[i] += s[i];
        dp_psd_accumulate (w, y, NN);
      }
    const double n        = (double)NN;
    const double med_bias = 10.0 * log10 (1.0 - 1.0 / (3.0 * (double)K));
    const double floor_sd = sqrt (M_PI / 2.0) / sqrt (n * (double)K);
    const double cross_sd = sqrt (2.0 * var / n) / sqrt ((double)K);
    const double want_nf  = 10.0 * log10 (var / n) + med_bias;
    const double want_snr = 10.0 * log10 (1.0 + n / var) - med_bias;
    const double tol_nf   = 10.0 * log10 (1.0 + 5.0 * floor_sd);
    const double tol_snr
        = 10.0
          * log10 (1.0
                   + 5.0 * sqrt (floor_sd * floor_sd + cross_sd * cross_sd));
    const double hz_per_bin = 1.0 / n;
    DP_CHECK (fabs (dp_psd_noise_floor (w) - want_nf) < tol_nf);
    DP_CHECK (
        fabs (dp_psd_snr (w, (kt - 3) * hz_per_bin, (kt + 3) * hz_per_bin)
              - want_snr)
        < tol_snr);
    free (y);
    free (s);
    dp_psd_destroy (w);
  }

  /* T14b: it is the MEDIAN.  T14's level cannot tell a median from a mean
   * of the dB values -- for K = 256 they differ by ~0.003 dB.  Here 32 of
   * the 256 bins carry a bin-centred unit tone (24 dB above the noise; rect,
   * so no leakage).  The median of 256 values with 32 high is the average
   * of order statistics 128 and 129 of the 224 noise bins: their quantile
   * 128.5/224 = 0.574, which for Gamma(K) is (1 - 1/(9K) + z sqrt(1/(9K)))^3
   * = 1.0103 of the mean (Wilson-Hilferty, z = 0.1857): the floor moves UP by
   * 0.045 dB.  That order statistic's sd is sqrt(p(1-p)/224) / phi(z) of
   * the per-bin sd 1/sqrt(K): 0.53%, so z = 5 gives 0.114 dB.  A mean of
   * the dB values would sit 32/256 x 24 = 3 dB high. */
  {
    const size_t    NN = 256, K = 256;
    const double    var  = 1.0;
    uint32_t        seed = 19120u;
    dp_psd_state_t *w    = dp_psd_create (NN, 1.0, 3, 0.0f, 1, 1.0, 0, 0, 0.0);
    float _Complex *y    = malloc (NN * sizeof *y);
    float _Complex *t    = malloc (NN * sizeof *t);
    float _Complex *s    = malloc (NN * sizeof *s);
    DP_REQUIRE (w && y && t && s);
    for (size_t i = 0; i < NN; i++)
      s[i] = 0.0f;
    for (int j = 0; j < 32; j++)
      {
        fill_tone (t, NN, 8 * j - 124); /* every 8th bin, -124 .. +124 */
        for (size_t i = 0; i < NN; i++)
          s[i] += t[i];
      }
    for (size_t f = 0; f < K; f++)
      {
        fill_cnoise (y, NN, var, &seed);
        for (size_t i = 0; i < NN; i++)
          y[i] += s[i];
        dp_psd_accumulate (w, y, NN);
      }
    const double p    = 128.5 / 224.0;
    const double z    = 0.1857; /* Phi^-1(0.5737) */
    const double phi  = 0.3921; /* standard normal density at z */
    const double c    = 1.0 / (9.0 * (double)K);
    const double q    = pow (1.0 - c + z * sqrt (c), 3.0);
    const double sd   = sqrt (p * (1.0 - p) / 224.0) / phi / sqrt ((double)K);
    const double want = 10.0 * log10 (var / (double)NN) + 10.0 * log10 (q);
    DP_CHECK (fabs (dp_psd_noise_floor (w) - want)
              < 10.0 * log10 (1.0 + 5.0 * sd));
    free (y);
    free (t);
    free (s);
    dp_psd_destroy (w);
  }

  /* T16: SFDR is the carrier minus the strongest spur, and 0 with fewer than
   * two peaks.  Bin-centred tones at 0 dB and -20 dB under Hann, placed half
   * the transform apart (bins -16 and +16).  Each tone's own neighbours sit
   * symmetrically at -6 dB, so find_peaks' parabolic correction vanishes; and
   * at N/2 apart the OTHER tone's leakage is symmetric about it too.  That
   * placement matters and was measured, with a double-precision model of
   * this readout (window, FFT, cg^2, find_peaks): at bins 6 and 20 the
   * carrier's -82 dBc leakage moves the spur by 5.1e-3 dB, at -16/+16 the
   * model reads 20 dB to 4e-15.  What is left is the float32 FFT: an error
   * of ~c log2(nfft) u ||X|| (see T8), which on a spur at 0.1 of the
   * carrier is ~4e-5 relative, 4e-4 dB.  Tolerance 2e-3 dB.  A single tone
   * leaves one peak above -30 dB (its main-lobe skirt is monotonic, not a
   * peak): exactly 0. */
  {
    dp_psd_state_t *w = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.0);
    dp_psd_state_t *o = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.0);
    DP_REQUIRE (w && o);
    float _Complex c[64], sp[64], x[64];
    fill_tone (c, N, -16);
    fill_tone (sp, N, 16);
    for (size_t i = 0; i < N; i++)
      x[i] = c[i] + 0.1f * sp[i];
    dp_psd_accumulate (w, x, N);
    DP_CHECK (fabs (dp_psd_sfdr (w, -30.0f) - 20.0) < 2e-3);
    dp_psd_accumulate (o, c, N);
    DP_CHECK (dp_psd_sfdr (o, -30.0f) == 0.0);
    dp_psd_destroy (w);
    dp_psd_destroy (o);
  }

  /* T17: create's remaining arguments.  beta shapes only the Kaiser window:
   * Hann, Blackman-Harris and rect are bit-identical at beta 0 and 8, while
   * Kaiser is not (the precondition: beta is live).  A negative mode is
   * refused like an out-of-range one, and n = 2 -- the stated minimum -- is
   * accepted. */
  {
    for (int win = 0; win <= 3; win++)
      {
        dp_psd_state_t *b0
            = dp_psd_create (N, 1.0, win, 0.0f, 1, 1.0, 0, 0, 0.0);
        dp_psd_state_t *b8
            = dp_psd_create (N, 1.0, win, 8.0f, 1, 1.0, 0, 0, 0.0);
        DP_REQUIRE (b0 && b8);
        const int same = memcmp (b0->w, b8->w, N * sizeof b0->w[0]) == 0;
        DP_CHECK (win == 1 ? !same : same);
        dp_psd_destroy (b0);
        dp_psd_destroy (b8);
      }
    DP_CHECK (dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, -1, 0.0) == NULL);
    dp_psd_state_t *two = dp_psd_create (2, 1.0, 3, 0.0f, 1, 1.0, 0, 0, 0.0);
    DP_CHECK (two != NULL);
    dp_psd_destroy (two);
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
            /* NaN-filled: at pad 2 nfft > n, and a kernel that wrote only n
               bins must fail every time, not by what malloc left there */
            for (size_t i = 0; i < nfft; i++)
              got_db[i] = got_p[i] = NAN;

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

  /* ── dp_psd_frame_linear: the kernel against the reference, linear ──────
   * The Spectrogram's mode = power (#1894) averages frames itself, so it
   * needs a frame in full-scale^2 units; the raw dp_psd_frame_power() sits
   * 20*log10(sum(w)) above that.  frame_linear and frame_db read one double
   * quotient, so:
   *  (i)   10*log10(linear) is frame_db wherever frame_db is above the floor.
   *        Tolerance: the linear value's float rounding moves its log by
   *        10/ln(10) * 2^-24 = 2.6e-7 dB, and the dB output's own float
   *        spacing is <= 200 * 2^-24 = 1.2e-5 dB: 2e-5 dB covers both.
   *  (ii)  a full-scale tone on a bin reads 1.0 whatever the window, padded
   *        or not, against full_scale and bits alike.  Tolerance: the float
   *        FFT's relative power error ~2 c log2(nfft) 2^-24 (c = 5), 8e-6 at
   *        nfft = 128; 2e-5.  A slip in the reference (cg vs n, a missing
   *        full_scale) moves it by a factor, not a ulp.
   *  (iii) the raw power is linear * cg^2 * full_scale^2, to 4 roundings.
   *  (iv)  the running average is untouched. */
  {
    uint32_t seed = 1911u * 3u;
    for (int win = 0; win <= 3; win++)
      for (size_t pad = 1; pad <= 2; pad++)
        {
          dp_psd_state_t *w
              = dp_psd_create (N, 1.0, win, 8.0f, pad, 1.0, 0, 0, 0.0);
          DP_REQUIRE (w != NULL);
          const size_t nfft = w->nfft;
          float _Complex x[64];
          for (size_t i = 0; i < N; i++)
            x[i] = dp_cgauss (&seed);
          float *lin = malloc (nfft * sizeof *lin);
          float *db  = malloc (nfft * sizeof *db);
          float *raw = malloc (nfft * sizeof *raw);
          DP_REQUIRE (lin && db && raw);
          for (size_t i = 0; i < nfft; i++)
            lin[i] = db[i] = raw[i] = NAN; /* see the kernel block above */
          dp_psd_frame_linear (w, x, lin);
          dp_psd_frame_db (w, x, db);
          dp_psd_frame_power (w, x, raw);
          const double ref   = w->cg * w->cg;
          int          as_db = 1, as_raw = 1, written = 1;
          for (size_t i = 0; i < nfft; i++)
            {
              written &= isfinite (lin[i]) && isfinite (db[i])
                         && isfinite (raw[i]);
              if (db[i] > -199.0f
                  && fabs (10.0 * log10 ((double)lin[i]) - (double)db[i])
                         > 2e-5)
                as_db = 0;
              if (fabs ((double)lin[i] * ref - (double)raw[i])
                  > 4.0 * ldexp (1.0, -24) * (double)raw[i])
                as_raw = 0;
            }
          DP_CHECK (written); /* every one of nfft bins, all three */
          DP_CHECK (as_db);   /* (i) */
          DP_CHECK (as_raw);  /* (iii) */

          /* (ii): a unit tone on bin 5 reads 1.0 at nfft/2 + 5*nfft/n */
          float _Complex t[64];
          fill_tone (t, N, 5);
          dp_psd_frame_linear (w, t, lin);
          DP_CHECK (fabs ((double)lin[nfft / 2 + 5 * (nfft / N)] - 1.0)
                    < 2e-5);
          DP_CHECK (w->avg->count == 0); /* (iv) */
          free (lin);
          free (db);
          free (raw);
          dp_psd_destroy (w);
        }

    /* (ii) against the reference: an amplitude-2^11 tone reads 1.0 with
     * full_scale = 2^11, and bits = 12 gives the same bytes. */
    dp_psd_state_t *wf = dp_psd_create (N, 1.0, 0, 0.0f, 1, 2048.0, 0, 0, 0.0);
    dp_psd_state_t *wb = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 12, 0, 0.0);
    DP_REQUIRE (wf && wb);
    float _Complex t[64];
    fill_tone (t, N, 5);
    for (size_t i = 0; i < N; i++)
      t[i] *= 2048.0f;
    float lf[64], lb[64];
    dp_psd_frame_linear (wf, t, lf);
    dp_psd_frame_linear (wb, t, lb);
    DP_CHECK (fabs ((double)lf[N / 2 + 5] - 1.0) < 2e-5);
    DP_CHECK (memcmp (lf, lb, sizeof lf) == 0);
    dp_psd_destroy (wf);
    dp_psd_destroy (wb);

    /* (v) frame_db, bit for bit, from the DOCUMENTED reference, computed
     * here rather than by the library.  Every other frame_db check compares
     * it with something that goes through the same reader (psd_db,
     * frame_linear), so a wrong reference moves both and they still agree.
     * This one takes only the raw |X|^2 from frame_power and applies the
     * header's definition itself: 10*log10 of raw / (cg^2 * full_scale^2),
     * formed in double, floored at 1e-20 (-200 dB), rounded once to float.
     * full_scale 2048 is a power of two, so the product is exact in any
     * order; zeros reach the floor. */
    static const double fss[] = { 1.0, 2048.0 };
    for (int win = 0; win <= 3; win++)
      for (size_t pad = 1; pad <= 2; pad++)
        for (size_t f = 0; f < 2; f++)
          for (int zero = 0; zero <= 1; zero++)
            {
              dp_psd_state_t *v
                  = dp_psd_create (N, 1.0, win, 8.0f, pad, fss[f], 0, 0, 0.0);
              DP_REQUIRE (v != NULL);
              const size_t nfft = v->nfft;
              float _Complex xv[64];
              for (size_t i = 0; i < N; i++)
                xv[i] = zero ? 0.0f : (float)fss[f] * dp_cgauss (&seed);
              float *raw = malloc (nfft * sizeof *raw);
              float *db  = malloc (nfft * sizeof *db);
              DP_REQUIRE (raw && db);
              for (size_t i = 0; i < nfft; i++)
                raw[i] = db[i] = NAN; /* see the kernel block above */
              dp_psd_frame_power (v, xv, raw);
              dp_psd_frame_db (v, xv, db);
              const double ref = (v->cg * v->cg) * (fss[f] * fss[f]);
              int          bit = 1;
              for (size_t i = 0; i < nfft; i++)
                {
                  const float want
                      = (float)(10.0
                                * log10 (fmax ((double)raw[i] / ref, 1e-20)));
                  if (memcmp (&want, &db[i], sizeof want) != 0)
                    bit = 0;
                }
              DP_CHECK (bit);
              free (raw);
              free (db);
              dp_psd_destroy (v);
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

  /* ════════════════════════════════════════════════════════════════════════
   * The per-frame kernel, certified (#1911, part a).  The Spectrogram
   * composes dp_psd_frame_db (#1894), so the claims it stands on are pinned
   * before the rest of PSD: the running average is untouched (T1), 0 dBFS
   * under every window and both references (T2), the -200 dB floor (T3), the
   * DC-centred layout for a negative bin and a padded transform (T4), ENBW
   * (T5) and the transform length (T18).
   * ════════════════════════════════════════════════════════════════════════
   */

  /* T1: both kernels leave a NON-EMPTY average untouched.  The check above
   * ran frame_db against an empty average only, where "untouched" and
   * "nothing there to touch" read the same, and never checked frame_power.
   * The probe frame is 20 dB louder than the averaged one, so any fold,
   * by the averager or by a write into its storage, moves the readout. */
  {
    uint32_t seed = 1911u;
    float _Complex a[64], b[64];
    for (size_t i = 0; i < N; i++)
      a[i] = dp_cgauss (&seed);
    for (size_t i = 0; i < N; i++)
      b[i] = 10.0f * dp_cgauss (&seed);
    dp_psd_state_t *w = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.0);
    DP_REQUIRE (w != NULL);
    dp_psd_accumulate (w, a, N);
    dp_psd_accumulate (w, a, N);
    const size_t count0 = w->avg->count;
    float        db0[64], p0[64], db1[64], p1[64], scratch[64];
    DP_REQUIRE (dp_psd_psd_db (w, N, db0, N) == N);
    DP_REQUIRE (dp_psd_power_twosided (w, N, p0, N) == N);

    dp_psd_frame_db (w, b, scratch);
    dp_psd_frame_power (w, b, scratch);

    DP_CHECK (w->avg->count == count0);
    DP_CHECK (dp_psd_psd_db (w, N, db1, N) == N);
    DP_CHECK (dp_psd_power_twosided (w, N, p1, N) == N);
    DP_CHECK (memcmp (db0, db1, sizeof db0) == 0);
    DP_CHECK (memcmp (p0, p1, sizeof p0) == 0);
    dp_psd_destroy (w);
  }

  /* T2: a full-scale tone on a bin reads 0 dBFS WHATEVER THE WINDOW, in the
   * kernel and in the averaged readout, unpadded and padded, and against
   * either reference.  It was asserted for the rectangular window only.
   *
   * Tolerance.  A bin-centred tone of amplitude A has |X[k]| = A * cg
   * exactly in real arithmetic.  A float32 FFT of nfft points carries a
   * relative amplitude error of order log2(nfft) * 2^-24 (~4e-7 at nfft =
   * 128), i.e. ~4e-6 dB.  TOL_DB = 1e-4 dB is 25x that, and four orders of
   * magnitude below what it guards against: a missing cg or full_scale
   * factor moves the reading by whole dB (Hann's cg^2 vs n^2 alone is
   * 6 dB). */
  {
    const float TOL_DB = 1e-4f;
    const int   k      = 5;
    for (int win = 0; win <= 3; win++)
      for (size_t pad = 1; pad <= 2; pad++)
        {
          dp_psd_state_t *w
              = dp_psd_create (N, 1.0, win, 8.0f, pad, 1.0, 0, 0, 0.0);
          DP_REQUIRE (w != NULL);
          const size_t nfft = w->nfft;
          /* bin k of the n-point frame is bin k*nfft/n of the transform */
          const size_t at = nfft / 2 + (size_t)k * (nfft / N);
          float _Complex x[64];
          fill_tone (x, N, k);
          float *db = malloc (nfft * sizeof *db);
          DP_REQUIRE (db != NULL);
          dp_psd_frame_db (w, x, db);
          DP_CHECK (fabsf (db[at]) < TOL_DB);
          dp_psd_accumulate (w, x, N);
          DP_CHECK (dp_psd_psd_db (w, nfft, db, nfft) == nfft);
          DP_CHECK (fabsf (db[at]) < TOL_DB);
          free (db);
          dp_psd_destroy (w);
        }

    /* The reference.  A tone of amplitude FS reads 0 dBFS against
     * full_scale = FS, and bits = B is the SAME reference as full_scale =
     * 2^(B-1) -- bit for bit, with the full_scale argument then ignored
     * (999 here: were it used, the tone would read +6.2 dB).  Scaling the
     * unit tone by 2^11 is exact in float32. */
    const size_t    B  = 12;
    const double    FS = 2048.0; /* 2^(B-1) */
    dp_psd_state_t *wf = dp_psd_create (N, 1.0, 0, 0.0f, 1, FS, 0, 0, 0.0);
    dp_psd_state_t *wb = dp_psd_create (N, 1.0, 0, 0.0f, 1, 999.0, B, 0, 0.0);
    DP_REQUIRE (wf != NULL && wb != NULL);
    DP_CHECK (wb->full_scale == FS);
    float _Complex x[64];
    fill_tone (x, N, k);
    for (size_t i = 0; i < N; i++)
      x[i] *= (float)FS;
    float df[64], dbits[64];
    dp_psd_frame_db (wf, x, df);
    dp_psd_frame_db (wb, x, dbits);
    DP_CHECK (fabsf (df[N / 2 + (size_t)k]) < TOL_DB);
    DP_CHECK (memcmp (df, dbits, sizeof df) == 0);
    dp_psd_accumulate (wf, x, N);
    DP_CHECK (dp_psd_psd_db (wf, N, df, N) == N);
    DP_CHECK (fabsf (df[N / 2 + (size_t)k]) < TOL_DB);
    dp_psd_destroy (wf);
    dp_psd_destroy (wb);
  }

  /* T3: the -200 dB floor.  10*log10 of an empty bin is guarded at 1e-20,
   * so an all-zero frame reads -200 dB in every bin: through the kernel,
   * the averaged readout and an integrated band.  Tolerance: 10*log10 of
   * the double 1e-20, cast to float, is -200 to within 2^-24 relative
   * (~1.2e-5 dB); 1e-4 dB covers it.  A floor at any other power of ten
   * misses by >= 10 dB. */
  {
    dp_psd_state_t *w = dp_psd_create (N, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.0);
    DP_REQUIRE (w != NULL);
    float _Complex zero[64];
    for (size_t i = 0; i < N; i++)
      zero[i] = 0.0f;
    float db[64];
    int   at_floor = 1;
    dp_psd_frame_db (w, zero, db);
    for (size_t i = 0; i < N; i++)
      if (fabsf (db[i] + 200.0f) > 1e-4f)
        at_floor = 0;
    DP_CHECK (at_floor);

    dp_psd_accumulate (w, zero, N);
    DP_CHECK (dp_psd_psd_db (w, N, db, N) == N);
    at_floor = 1;
    for (size_t i = 0; i < N; i++)
      if (fabsf (db[i] + 200.0f) > 1e-4f)
        at_floor = 0;
    DP_CHECK (at_floor);

    const double band[2] = { -0.25, 0.25 }; /* in span, all-zero power */
    float        pb[1];
    DP_CHECK (dp_psd_band_power (w, band, 2, pb, 1) == 1);
    DP_CHECK (fabsf (pb[0] + 200.0f) < 1e-4f);
    DP_CHECK (fabs (dp_psd_total_band_power (w, band, 2) + 200.0) < 1e-4);
    dp_psd_destroy (w);
  }

  /* T4: the DC-centred layout for a NEGATIVE bin and a PADDED transform.
   * Bin k of an n-point frame is frequency k/n; in an nfft-point transform
   * that is bin k*nfft/n, stored at nfft/2 + k*nfft/n.  The tests above
   * placed positive k with nfft == n only.  Rectangular, so a bin-centred
   * tone has no leakage and its peak index is exact; the claim is integer
   * and needs no tolerance. */
  {
    static const int ks[] = { -7, -1, 3 };
    for (size_t pad = 1; pad <= 2; pad++)
      for (size_t j = 0; j < sizeof ks / sizeof ks[0]; j++)
        {
          dp_psd_state_t *w
              = dp_psd_create (N, 1.0, 3, 0.0f, pad, 1.0, 0, 0, 0.0);
          DP_REQUIRE (w != NULL);
          const size_t nfft = w->nfft;
          const long   step = (long)(nfft / N);
          const size_t want = (size_t)((long)(nfft / 2) + ks[j] * step);
          float _Complex x[64];
          fill_tone (x, N, ks[j]);
          float *db = malloc (nfft * sizeof *db);
          DP_REQUIRE (db != NULL);
          dp_psd_frame_db (w, x, db);
          DP_CHECK (argmax (db, nfft) == want);
          dp_psd_accumulate (w, x, N);
          DP_CHECK (dp_psd_psd_db (w, nfft, db, nfft) == nfft);
          DP_CHECK (argmax (db, nfft) == want);
          free (db);
          dp_psd_destroy (w);
        }
  }

  /* T5: ENBW against a truth that is not the code's own formula.
   *
   * Rectangular: sum(w) = sum(w^2) = n, so n*s2/cg^2 is exactly 1.0 in
   * double, and nothing short of an exact 1.0 is correct.
   *
   * Blackman-Harris, the 4-term minimum window, from its PUBLISHED
   * coefficients (Harris 1978, Table 1, where the periodic window's ENBW is
   * tabulated as 2.00 bins).  doppler's window is the symmetric one (it
   * divides by N-1): the periodic (N-1)-point window plus one end sample
   * w0 = a0 - a1 + a2 - a3.  Every cosine product in w^2 has a frequency of
   * at most 6 cycles, so for N-1 > 6 its sum over the period vanishes and
   *   sum(w)   = (N-1) a0 + w0
   *   sum(w^2) = (N-1) P  + w0^2,   P = a0^2 + (a1^2 + a2^2 + a3^2) / 2
   * exactly (measured: the double closed form and a double-precision window
   * agree to 1e-15).  Tolerance: the window is stored in float32, and that
   * rounding moves ENBW by 4e-9 at N = 64 and 3e-9 at N = 100 (measured);
   * 1e-6 absolute is 250x that, and still 19x tighter than a coefficient
   * off in its 5th digit (a2 = 0.14128 -> 0.1413 moves ENBW by 1.9e-5). */
  {
    const double a0 = 0.35875, a1 = 0.48829, a2 = 0.14128, a3 = 0.01168;
    const double P  = a0 * a0 + (a1 * a1 + a2 * a2 + a3 * a3) / 2.0;
    const double w0 = a0 - a1 + a2 - a3;
    DP_CHECK (fabs (P / (a0 * a0) - 2.00) < 0.005); /* Harris's 2.00 */
    static const size_t ns[] = { 64, 100 };
    for (size_t j = 0; j < 2; j++)
      {
        const double    n = (double)ns[j];
        dp_psd_state_t *r
            = dp_psd_create (ns[j], 1.0, 3, 0.0f, 1, 1.0, 0, 0, 0.0);
        dp_psd_state_t *bh
            = dp_psd_create (ns[j], 1.0, 2, 0.0f, 1, 1.0, 0, 0, 0.0);
        DP_REQUIRE (r != NULL && bh != NULL);
        DP_CHECK (r->enbw == 1.0);
        const double want = n * ((n - 1.0) * P + w0 * w0)
                            / (((n - 1.0) * a0 + w0) * ((n - 1.0) * a0 + w0));
        DP_CHECK (fabs (bh->enbw - want) < 1e-6);
        dp_psd_destroy (r);
        dp_psd_destroy (bh);
      }
  }

  /* T18: the transform length is next_pow_two(n * pad).  Rectangular so a
   * tiny n stays a well-defined window (a symmetric Hann of 2 points is all
   * zeros). */
  {
    static const struct
    {
      size_t n, pad, nfft;
    } c[] = {
      { 64, 1, 64 },  { 64, 4, 256 }, { 100, 1, 128 }, { 100, 3, 512 },
      { 65, 1, 128 }, { 33, 2, 128 }, { 2, 1, 2 },
    };
    for (size_t j = 0; j < sizeof c / sizeof c[0]; j++)
      {
        dp_psd_state_t *w
            = dp_psd_create (c[j].n, 1.0, 3, 0.0f, c[j].pad, 1.0, 0, 0, 0.0);
        DP_REQUIRE (w != NULL);
        DP_CHECK (w->nfft == c[j].nfft);
        dp_psd_destroy (w);
      }
  }

  /* T19: every reader and every *_max_out is sized by nfft, not n (#1911
   * (a), F4).  At pad = 1, nfft == n, so a reader or a hint that used n
   * passed every test above; here n = 64 and pad = 2 give nfft = 128.  Each
   * hint must equal its reader's documented length, and each reader, given
   * room for twice that, must write exactly that many finite floats and
   * leave the rest of a NaN-filled buffer untouched.  band_power's hint is
   * 0: the binding sizes its output from the bands. */
  {
    const size_t    n = 64, nfft = 128, half = nfft / 2;
    dp_psd_state_t *w = dp_psd_create (n, 1.0, 0, 0.0f, 2, 1.0, 0, 0, 0.0);
    DP_REQUIRE (w != NULL && w->nfft == nfft);
    float _Complex x[64];
    fill_tone (x, n, 5);
    dp_psd_accumulate (w, x, n);
    DP_CHECK (dp_psd_power_twosided_max_out (w) == nfft);
    DP_CHECK (dp_psd_power_onesided_max_out (w) == half + 1);
    DP_CHECK (dp_psd_psd_db_max_out (w) == nfft);
    DP_CHECK (dp_psd_psd_dbhz_max_out (w) == nfft);
    DP_CHECK (dp_psd_band_power_max_out (w) == 0);
    struct
    {
      const char *name;
      size_t      want;
    } r[] = { { "power_twosided", nfft },
              { "power_onesided", half + 1 },
              { "psd_db", nfft },
              { "psd_dbhz", nfft } };
    for (size_t j = 0; j < sizeof r / sizeof r[0]; j++)
      {
        float  buf[2 * 128];
        size_t got = 0;
        for (size_t i = 0; i < 2 * nfft; i++)
          buf[i] = NAN;
        if (j == 0)
          got = dp_psd_power_twosided (w, 2 * nfft, buf, 2 * nfft);
        else if (j == 1)
          got = dp_psd_power_onesided (w, 2 * nfft, buf, 2 * nfft);
        else if (j == 2)
          got = dp_psd_psd_db (w, 2 * nfft, buf, 2 * nfft);
        else
          got = dp_psd_psd_dbhz (w, 2 * nfft, buf, 2 * nfft);
        int written = 1, untouched = 1;
        for (size_t i = 0; i < r[j].want; i++)
          written &= isfinite (buf[i]) != 0;
        for (size_t i = r[j].want; i < 2 * nfft; i++)
          untouched &= isnan (buf[i]) != 0;
        if (got != r[j].want || !written || !untouched)
          printf ("T19 %s: returned %zu (want %zu), written %d, "
                  "untouched %d\n",
                  r[j].name, got, r[j].want, written, untouched);
        DP_CHECK (got == r[j].want);
        DP_CHECK (written);
        DP_CHECK (untouched);
      }
    dp_psd_destroy (w);
  }

  /* T20: band_power with no complete lo/hi pair reports nothing, after a
   * frame too (psd_core.h, band_power: "0 when @p bands holds no complete
   * lo/hi pair").  Zero edges and one edge each return 0 and write nothing;
   * the precondition is that a complete pair on the same estimator does
   * write one band. */
  {
    dp_psd_state_t *w = dp_psd_create (64, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.0);
    DP_REQUIRE (w != NULL);
    float _Complex x[64];
    fill_tone (x, 64, 3);
    dp_psd_accumulate (w, x, 64);
    const double edges[2] = { -0.25, 0.25 };
    for (size_t len = 0; len < 2; len++)
      {
        float out[2] = { NAN, NAN };
        DP_CHECK (dp_psd_band_power (w, edges, len, out, 2) == 0);
        DP_CHECK (isnan (out[0]) && isnan (out[1]));
      }
    float one[2] = { NAN, NAN };
    DP_CHECK (dp_psd_band_power (w, edges, 2, one, 2) == 1);
    DP_CHECK (isfinite (one[0]) && isnan (one[1]));
    dp_psd_destroy (w);
  }

  DP_TEST_END ("test_psd_core");
}
