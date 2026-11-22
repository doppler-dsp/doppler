/**
 * @file wlan_tx_quality.c
 * @brief Transmit quality of the 802.11 non-HT PPDU against window and
 *        oversampling: the spectral mask (17.3.9.3), the error the window
 *        adds to a symbol body, the L-LTF's spectral flatness (17.3.9.7.3)
 *        and PAPR (docs/design/wlan.md section 7, its fourth proof, and
 *        section 8).
 *
 * Every PPDU is the library's own (dp_wlan_ppdu, the standard tables, seeded
 * PSDUs); every spectrum is the library's PSD kernel (Hann, periodic); every
 * transform is the library's fft. The harness renders nothing itself.
 *
 * MASK. A Welch average of Hann frames of 512 M samples (bins of 39.0625
 * kHz at fs = 20 M MHz), zero-padded 8 times (bins of 4.88 kHz), hop 256 M,
 * taken only INSIDE one PPDU's SIGNAL and
 * Data samples: a frame never crosses from one PPDU to the next, because that
 * join is a cut no window smooths, and once per PPDU it would set the skirt
 * the windowed rows are trying to read. The 100 kHz resolution bandwidth is
 * realized by INTEGRATING: the level at f is the sum of the averaged padded
 * bins whose centres lie within 50 kHz of f (20 bins, 97.7 kHz realized, a
 * power-of-two transform cannot land on 100 kHz exactly at these rates; the
 * harness prints the width), against the largest such sum whose
 * centre is in band (|f| < 8.2 MHz). The Hann kernel's own 58.6 kHz ENBW
 * smears that rectangle by under a bin. A level is dBr against the in-band
 * peak, as the mask is written. A second row per configuration takes the
 * frames over the whole PPDU (preamble included) for comparison.
 *
 * The standard error of one level, in dB, for K frames of 50% overlapped
 * Hann (Welch 1967; the overlap correlation of Hann at half a frame is
 * rho = 1/6, so the variance is (1 + 2 rho^2) / K of one periodogram's), not
 * crediting the integration over bins:
 *   se_db = (10 / ln 10) * sqrt ((1 + 2 / 36) / K).
 * The harness prints K and se_db beside each level.
 *
 * EVM. Each SIGNAL and Data symbol's 64 M-sample transform, starting `start`
 * samples into the symbol (16 M: the last 64 M, a receiver's usual timing;
 * 8 M: eight 20 MHz samples early), is compared on its 52 occupied
 * subcarriers with the same transform of the UNWINDOWED render at the same
 * timing, so any known phase the timing puts on the grid cancels. Two checks:
 *   - exactness: for window <= start the bodies are the same samples, so the
 *     error is exactly zero;
 *   - Parseval: the unwindowed body is the IDFT of its grid, all its energy
 *     on the occupied subcarriers, so the occupied-subcarrier error is at
 *     most the time-domain error energy over the same samples:
 *     EVM^2 <= sum |x_w - x_0|^2 / sum |x_0|^2, exactly.
 * The expectation printed beside it, EVM^2 = 2 sum_{j=start}^{w-1} fall_j^2
 * / (64 M), assumes independent unit-power symbols (the prediction).
 *
 * FLATNESS. The L-LTF's second long symbol, transformed: each occupied
 * subcarrier's power in dB against the mean. Printed, not asserted.
 *
 * PAPR. Per Data symbol body (64 M samples): the fraction above 6, 8, 10 and
 * 12 dB with a Wilson 95% interval. Printed, not asserted.
 *
 *   wlan_tx_quality              the full sweep, printed
 *   wlan_tx_quality --emit       the full sweep as CSV blocks
 *   wlan_tx_quality --check      the subset ctest runs (M = 4, w in {0, 8,
 *                                16, 32} for the EVM checks; the mask at
 *                                w = 0 and 2 M over two PPDUs, with the
 *                                fast pin below)
 *   wlan_tx_quality --check --emit
 */
#include "doppler/fft/fft_core.h"
#include "doppler/psd/psd_core.h"
#include "doppler/wlan/wlan_core.h"
#include "dp_rng_test.h"
#include "dp_stats_test.h"
#include "dp_test.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PSDU_LEN 4095u

typedef float _Complex cf;

static int emit_csv;

/* One PPDU of PSDU_LEN seeded octets at rate r, window w, oversample m. */
static cf *
ppdu (const dp_wlan_rate_t *r, unsigned w, unsigned m, uint32_t seed,
      size_t *n_out, size_t *n_sym)
{
  static uint8_t psdu[PSDU_LEN];
  for (size_t i = 0; i < PSDU_LEN; i++)
    psdu[i] = (uint8_t)(dp_xs32 (&seed) >> 24);
  *n_sym         = dp_wlan_rate_n_sym (r, PSDU_LEN);
  const size_t n = dp_wlan_ppdu_len (*n_sym, w, m);
  cf          *x = malloc (n * sizeof *x);
  if (!x
      || dp_wlan_ppdu (dp_wlan_tables_standard (), r, psdu, PSDU_LEN,
                       (uint8_t)(1u + seed % 127u), w, m, x, n)
             != n)
    {
      fprintf (stderr, "wlan_tx_quality: render refused (w %u, M %u)\n", w, m);
      exit (2);
    }
  *n_out = n;
  return x;
}

/* ── the mask ───────────────────────────────────────────────────────────── */

typedef struct
{
  double rbw_hz;   /* the integrated width actually realized */
  double level[3]; /* dBr at 11, 20, 30 MHz: the larger of the two sides */
  double peak_over_mean_db;
  size_t frames;
} mask_t;

static mask_t
mask_levels (const dp_wlan_rate_t *r, unsigned m, unsigned w, unsigned ppdus,
             int whole)
{
  const size_t    n = 512u * m, hop = n / 2u;
  dp_psd_kernel_t k;
  if (dp_psd_kernel_init (&k, n, DP_PSD_WINDOW_HANN, 0.0f, 8, 1.0) != DP_OK)
    exit (2);
  const size_t nfft = k.nfft;
  double      *avg  = calloc (nfft, sizeof *avg);
  float       *lin  = malloc (nfft * sizeof *lin);
  mask_t       out  = { 0, { 0, 0, 0 }, 0, 0 };
  for (unsigned p = 0; p < ppdus; p++)
    {
      size_t       len, n_sym;
      cf          *x = ppdu (r, w, m, 1000u + p, &len, &n_sym);
      const size_t a = whole ? 0 : (size_t)m * 320u;
      const size_t b = (size_t)m * (400u + 80u * n_sym);
      for (size_t s = a; s + n <= b; s += hop)
        {
          dp_psd_frame_linear (&k, x + s, lin);
          for (size_t i = 0; i < nfft; i++)
            avg[i] += lin[i];
          out.frames++;
        }
      free (x);
    }
  /* bin i is (i - nfft/2) * fs / nfft; integrate 100 kHz around f */
  const double fs = 20e6 * m, df = fs / (double)nfft;
  double       peak = 0, mean = 0;
  size_t       n_in = 0;
  const long   half = lround (50e3 / df);
  for (long c = -(long)(8.2e6 / df); c <= (long)(8.2e6 / df); c++)
    {
      double band = 0;
      for (long j = c - half; j < c + half; j++)
        band += avg[(size_t)(j + (long)nfft / 2)];
      if (band > peak)
        peak = band;
      mean += avg[(size_t)(c + (long)nfft / 2)];
      n_in++;
    }
  mean                      = mean / (double)n_in * (double)(2 * half);
  out.rbw_hz                = (double)(2 * half) * df;
  out.peak_over_mean_db     = 10.0 * log10 (peak / mean);
  static const double fz[3] = { 11e6, 20e6, 30e6 };
  for (int q = 0; q < 3; q++)
    {
      double worst = 0;
      for (int side = -1; side <= 1; side += 2)
        {
          const long c    = lround (side * fz[q] / df);
          double     band = 0;
          for (long j = c - half; j < c + half; j++)
            band += avg[(size_t)(j + (long)nfft / 2)];
          if (band > worst)
            worst = band;
        }
      out.level[q] = 10.0 * log10 (worst / peak);
    }
  free (avg);
  free (lin);
  dp_psd_kernel_free (&k);
  return out;
}

/* ── the EVM a window adds ──────────────────────────────────────────────── */

typedef struct
{
  double evm_db;      /* measured, occupied subcarriers */
  double parseval_db; /* the time-domain bound it must not exceed */
  double expect_db;   /* the expectation, independent unit-power symbols */
  int    exact;       /* 1 if the error is exactly zero */
} evm_t;

static double
fall_sum (unsigned w, unsigned start)
{
  double s = 0;
  for (unsigned j = start; j < w; j++)
    {
      const double r = sin (M_PI * (j + 0.5) / (2.0 * w));
      s += (1.0 - r * r) * (1.0 - r * r);
    }
  return s;
}

static evm_t
evm (const dp_wlan_rate_t *r, unsigned m, unsigned w, unsigned start,
     unsigned ppdus)
{
  const size_t    nt = 64u * m;
  dp_fft_state_t *f  = dp_fft_create (nt, -1, 1);
  cf             *a = malloc (nt * sizeof *a), *b = malloc (nt * sizeof *b);
  double          e_occ = 0, s_occ = 0, e_time = 0, s_time = 0;
  int             exact = 1;
  for (unsigned p = 0; p < ppdus; p++)
    {
      size_t len0, lenw, n_sym;
      cf    *x0 = ppdu (r, 0, m, 2000u + p, &len0, &n_sym);
      cf    *xw = ppdu (r, w, m, 2000u + p, &lenw, &n_sym);
      for (size_t s = 0; s < 1 + n_sym; s++)
        {
          const size_t at = (size_t)m * (320u + 80u * s) + start;
          for (size_t i = 0; i < nt; i++)
            {
              const cf d = xw[at + i] - x0[at + i];
              exact &= d == 0.0f;
              e_time += crealf (d) * crealf (d) + cimagf (d) * cimagf (d);
              s_time += crealf (x0[at + i]) * crealf (x0[at + i])
                        + cimagf (x0[at + i]) * cimagf (x0[at + i]);
            }
          dp_fft_execute_cf32 (f, x0 + at, nt, a, nt);
          dp_fft_execute_cf32 (f, xw + at, nt, b, nt);
          for (int k = -26; k <= 26; k++)
            {
              if (k == 0)
                continue;
              const size_t i = (size_t)((k + (int)nt) % (int)nt);
              const cf     d = b[i] - a[i];
              e_occ += crealf (d) * crealf (d) + cimagf (d) * cimagf (d);
              s_occ += crealf (a[i]) * crealf (a[i])
                       + cimagf (a[i]) * cimagf (a[i]);
            }
        }
      free (x0);
      free (xw);
    }
  free (a);
  free (b);
  dp_fft_destroy (f);
  evm_t out;
  out.exact         = exact;
  out.evm_db        = 10.0 * log10 (e_occ / s_occ);
  out.parseval_db   = 10.0 * log10 (e_time / s_time);
  const double fsum = fall_sum (w, start);
  out.expect_db     = 10.0 * log10 (2.0 * fsum / (64.0 * m));
  return out;
}

/* ── flatness and PAPR ──────────────────────────────────────────────────── */

static double
ltf_flatness_db (unsigned m, unsigned w)
{
  const dp_wlan_rate_t r = { 6, WLAN_CODE_3_4 };
  size_t               len, n_sym;
  cf                  *x  = ppdu (&r, w, m, 3000u, &len, &n_sym);
  const size_t         nt = 64u * m;
  dp_fft_state_t      *f  = dp_fft_create (nt, -1, 1);
  cf                  *X  = malloc (nt * sizeof *X);
  dp_fft_execute_cf32 (f, x + (size_t)m * (160u + 32u + 64u), nt, X, nt);
  double p[53], mean = 0;
  int    n = 0;
  for (int k = -26; k <= 26; k++)
    if (k)
      {
        const cf v = X[(size_t)((k + (int)nt) % (int)nt)];
        p[n]       = crealf (v) * crealf (v) + cimagf (v) * cimagf (v);
        mean += p[n++];
      }
  mean /= n;
  double worst = 0;
  for (int i = 0; i < n; i++)
    worst = fmax (worst, fabs (10.0 * log10 (p[i] / mean)));
  free (X);
  free (x);
  dp_fft_destroy (f);
  return worst;
}

static void
papr (const dp_wlan_rate_t *r, unsigned m, unsigned w, unsigned ppdus,
      const char *rate)
{
  static const double g_db[4] = { 6, 8, 10, 12 };
  size_t              over[4] = { 0, 0, 0, 0 }, total = 0;
  for (unsigned p = 0; p < ppdus; p++)
    {
      size_t len, n_sym;
      cf    *x = ppdu (r, w, m, 4000u + p, &len, &n_sym);
      for (size_t s = 0; s < n_sym; s++)
        {
          const cf *body = x + (size_t)m * (400u + 80u * s + 16u);
          double    pk = 0, mean = 0;
          for (size_t i = 0; i < 64u * m; i++)
            {
              const double v = crealf (body[i]) * crealf (body[i])
                               + cimagf (body[i]) * cimagf (body[i]);
              pk             = fmax (pk, v);
              mean += v;
            }
          mean /= 64.0 * m;
          const double db = 10.0 * log10 (pk / mean);
          for (int q = 0; q < 4; q++)
            over[q] += db > g_db[q];
          total++;
        }
      free (x);
    }
  for (int q = 0; q < 4; q++)
    {
      double lo, hi;
      dp_wilson ((double)over[q], (double)total, &lo, &hi);
      /* the cited model, P(PAPR > g) = 1 - (1 - e^-g)^(2.8 * 52) */
      const double g    = pow (10.0, g_db[q] / 10.0);
      const double pred = 1.0 - pow (1.0 - exp (-g), 2.8 * 52.0);
      if (emit_csv)
        printf ("%s,%u,%u,%.0f,%zu,%zu,%.4e,%.4e,%.4e,%.4e\n", rate, m, w,
                g_db[q], over[q], total, (double)over[q] / (double)total, lo,
                hi, pred);
      else
        printf ("  %s M=%u w=%3u  PAPR > %2.0f dB: %5zu / %zu = %.4f "
                "[%.4f, %.4f]  model %.4f\n",
                rate, m, w, g_db[q], over[q], total,
                (double)over[q] / (double)total, lo, hi, pred);
    }
}

int
main (int argc, char **argv)
{
  int check = 0;
  for (int i = 1; i < argc; i++)
    {
      check |= strcmp (argv[i], "--check") == 0;
      emit_csv |= strcmp (argv[i], "--emit") == 0;
    }
  const dp_wlan_rate_t r54   = { 6, WLAN_CODE_3_4 };
  const unsigned       ppdus = check ? 2u : 8u;

  /* EVM: the window, at two receiver timings, against the exact checks */
  {
    static const unsigned wm[] = { 0, 2, 8, 9, 12, 16, 17, 20, 24, 32, 80 };
    static const unsigned wc[] = { 0, 8, 16, 32 };
    const unsigned       *ws   = check ? wc : wm;
    const size_t          nw   = check ? 4u : sizeof wm / sizeof *wm;
    const unsigned        m    = 4;
    if (emit_csv)
      printf (
          "# evm\nm,window_m,start_m,evm_db,parseval_db,expect_db,exact\n");
    for (unsigned st = 8; st <= 16; st += 8)
      for (size_t i = 0; i < nw; i++)
        {
          const evm_t e = evm (&r54, m, ws[i] * m, st * m, check ? 1u : 2u);
          if (emit_csv)
            printf ("%u,%u,%u,%.3f,%.3f,%.3f,%d\n", m, ws[i], st, e.evm_db,
                    e.parseval_db, e.expect_db, e.exact);
          else
            printf ("  EVM M=%u w=%2uM start=%2uM: %8.2f dB (Parseval %8.2f, "
                    "expect %8.2f)%s\n",
                    m, ws[i], st, e.evm_db, e.parseval_db, e.expect_db,
                    e.exact ? " exact" : "");
          DP_CHECK_MSG (e.exact == (ws[i] <= st),
                        "the body is untouched exactly when w <= start");
          if (!e.exact)
            /* The margin is the float transforms' own error: at 64 M = 256
             * points their derived bound (ofdm_fft_error.c) is 1.2e-5 of
             * the amplitude, under 1e-3 dB on a power ratio. */
            DP_CHECK_MSG (e.evm_db <= e.parseval_db + 1e-3,
                          "the occupied-subcarrier error is within Parseval");
        }
  }

  /* the mask */
  {
    static const unsigned mm[] = { 4, 8 };
    static const unsigned wm[] = { 0, 1, 2, 4 }; /* times M */
    if (emit_csv)
      printf ("# mask\nm,window_m,span,frames,se_db,rbw_hz,"
              "peak_over_mean_db,dbr_11,dbr_20,dbr_30\n");
    for (size_t im = 0; im < (check ? 1u : 2u); im++)
      for (size_t iw = 0; iw < 4; iw++)
        for (int whole = 0; whole <= 1; whole++)
          {
            if (check && (iw == 1 || iw == 3 || whole))
              continue;
            const unsigned m = mm[im], w = wm[iw] * m;
            const mask_t   q  = mask_levels (&r54, m, w, ppdus, whole);
            const double   se = 10.0 / log (10.0)
                                * sqrt ((1.0 + 2.0 / 36.0) / (double)q.frames);
            /* The fast pin, chosen from the sweep's margins: unwindowed
             * fails the -40 dBr point at 30 MHz, and the standard's 2 M
             * transition passes all three points, each by at least three
             * standard errors of the level (se above). */
            static const double mask_db[3] = { -20.0, -28.0, -40.0 };
            if (!whole && wm[iw] == 0)
              DP_CHECK_MSG (q.level[2] - mask_db[2] >= 3.0 * se,
                            "unwindowed: above -40 dBr at 30 MHz by 3 se");
            if (!whole && wm[iw] == 2)
              for (int p = 0; p < 3; p++)
                DP_CHECK_MSG (mask_db[p] - q.level[p] >= 3.0 * se,
                              "window 2 M: under every mask point by 3 se");
            if (emit_csv)
              printf ("%u,%u,%s,%zu,%.3f,%.0f,%.3f,%.2f,%.2f,%.2f\n", m,
                      wm[iw], whole ? "ppdu" : "data", q.frames, se, q.rbw_hz,
                      q.peak_over_mean_db, q.level[0], q.level[1], q.level[2]);
            else
              printf ("  mask M=%u w=%uM %s K=%zu se=%.2f dB pk/mean=%.2f: "
                      "%.1f / %.1f / %.1f dBr (mask -20 / -28 / -40)\n",
                      m, wm[iw], whole ? "ppdu" : "data", q.frames, se,
                      q.peak_over_mean_db, q.level[0], q.level[1], q.level[2]);
          }
  }

  if (!check)
    {
      if (emit_csv)
        printf ("# flatness\nm,window_m,worst_db\n");
      for (unsigned w = 0; w <= 32; w += 8)
        {
          const double fl = ltf_flatness_db (4, w * 4);
          if (emit_csv)
            printf ("4,%u,%.4f\n", w, fl);
          else
            printf ("  L-LTF flatness M=4 w=%2uM: worst %.4f dB\n", w, fl);
        }
      if (emit_csv)
        printf ("# papr\nrate,m,window,gamma_db,over,symbols,rate_hat,lo,hi,"
                "model\n");
      const dp_wlan_rate_t r36 = { 4, WLAN_CODE_3_4 };
      papr (&r54, 4, 8, 8, "54");
      papr (&r36, 4, 8, 8, "36");
    }

  if (emit_csv)
    DP_TEST_EMIT_END ("wlan_tx_quality");
  DP_TEST_END (check ? "wlan_tx_quality --check" : "wlan_tx_quality");
}
