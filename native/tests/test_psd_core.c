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

  DP_TEST_END ("test_psd_core");
}
