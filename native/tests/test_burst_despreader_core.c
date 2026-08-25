#include "burst_despreader/burst_despreader_core.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Spread `nsym` BPSK bits by `code` (length sf), oversample by sps
 * (rectangular hold), and optionally rotate by a per-sample carrier `f0`
 * (cycles/sample). Returns a malloc'd cf32 burst of nsym*sf*sps samples; fills
 * tx_bits. */
static float complex *
make_burst (const uint8_t *code, size_t sf, size_t sps, size_t nsym, double f0,
            uint8_t *tx_bits, size_t *out_len)
{
  size_t         nsamp = nsym * sf * sps;
  float complex *x     = malloc (nsamp * sizeof (*x));
  size_t         k     = 0;
  for (size_t i = 0; i < nsym; i++)
    {
      uint8_t bit = (uint8_t)((i * 2654435761u) >> 31) & 1u; /* cheap PRBS */
      tx_bits[i]  = bit;
      float sym   = bit ? -1.0f : 1.0f; /* BPSK: 0->+1, 1->-1 */
      for (size_t j = 0; j < sf; j++)
        {
          float chip = sym * ((code[j] & 1u) ? -1.0f : 1.0f);
          for (size_t s = 0; s < sps; s++, k++)
            {
              float complex c
                  = cexpf ((float)(2.0 * M_PI * f0 * (double)k) * I);
              x[k] = chip * c;
            }
        }
    }
  *out_len = nsamp;
  return x;
}

/* Ambiguity-tolerant bit error count over [start, nsym): 180 deg is
 * don't-care, so a globally-inverted decision counts as correct. */
static double
amb_ber (const uint8_t *rx, const uint8_t *tx, size_t start, size_t nsym)
{
  size_t err = 0, tot = 0;
  for (size_t i = start; i < nsym; i++, tot++)
    err += (rx[i] != tx[i]);
  double b = (double)err / (double)tot;
  return b < 1.0 - b ? b : 1.0 - b;
}

int
main (void)
{

  /* Invalid args -> NULL (not a silent zero state). */
  DP_CHECK (burst_despreader_create (NULL, 0, 1, 2, 0.0, 0.0, 0.05, 0.01)
            == NULL);

  size_t  sf = 31, sps = 4, nsym = 120;
  uint8_t code[31];
  for (size_t i = 0; i < sf; i++)
    code[i] = (uint8_t)((i * 2246822519u) >> 31) & 1u;

  uint8_t *tx = malloc (nsym), *rx = malloc (nsym);
  size_t   blen = 0;

  /* (1) Genie: zero offset, no noise -> exact recovery. */
  float complex *burst = make_burst (code, sf, sps, nsym, 0.0, tx, &blen);
  burst_despreader_state_t *d
      = burst_despreader_create (code, sf, sf, sps, 0.0, 0.0, 0.05, 0.01);
  DP_CHECK (d != NULL);
  size_t n_out = burst_despreader_bits (d, burst, blen, rx, nsym);
  DP_CHECK (n_out == nsym);
  DP_CHECK (amb_ber (rx, tx, 0, n_out) == 0.0);
  burst_despreader_destroy (d);
  free (burst);

  /* (2) Carrier offset, seeded at the true frequency -> exact recovery,
   *     loop holds the frequency. */
  double f0 = 0.0006;
  burst     = make_burst (code, sf, sps, nsym, f0, tx, &blen);
  d         = burst_despreader_create (code, sf, sf, sps, f0, 0.0, 0.05, 0.01);
  n_out     = burst_despreader_bits (d, burst, blen, rx, nsym);
  DP_CHECK (amb_ber (rx, tx, n_out / 4, n_out) == 0.0);
  DP_CHECK (fabs (burst_despreader_get_norm_freq (d) - f0) < 1e-4);
  DP_CHECK (burst_despreader_get_lock_metric (d) > 0.9);

  /* (3) reset re-seeds; a second identical run reproduces the first. */
  burst_despreader_reset (d);
  uint8_t *rx2 = malloc (nsym);
  size_t   n2  = burst_despreader_bits (d, burst, blen, rx2, nsym);
  DP_CHECK (n2 == n_out);
  DP_CHECK (amb_ber (rx2, tx, n2 / 4, n2) == 0.0);

  /* (4) property accessors round-trip. */
  burst_despreader_set_bn_carrier (d, 0.06);
  DP_CHECK (burst_despreader_get_bn_carrier (d) == 0.06);
  burst_despreader_set_bn_code (d, 0.02);
  DP_CHECK (burst_despreader_get_bn_code (d) == 0.02);
  burst_despreader_set_norm_freq (d, 0.001);
  DP_CHECK (fabs (burst_despreader_get_norm_freq (d) - 0.001) < 1e-9);
  (void)burst_despreader_get_code_phase (d);
  (void)burst_despreader_get_lock_metric (d);
  (void)burst_despreader_get_snr_est (d);

  /* (5) set_acq enable then disable (payload-only). */
  uint8_t acq[16];
  for (size_t i = 0; i < 16; i++)
    acq[i] = (uint8_t)(i & 1u);
  burst_despreader_set_acq (d, acq, 16, 3);
  burst_despreader_set_acq (d, NULL, 0, 0); /* disable */

  burst_despreader_destroy (d);
  free (burst);
  free (rx2);

  /* (6) cumulative burst statistics: every prompt weighted equally,
   * snr_est calibrated against the known post-despread SNR, lock_stat
   * far above any gate on a live burst, and reset() re-arms. */
  {
    uint32_t st    = 42u;
    float    sigma = 1.0f; /* per-component input noise std (A = 1) */
    burst          = make_burst (code, sf, sps, nsym, 0.0, tx, &blen);
    for (size_t i = 0; i < blen; i++)
      {
        /* Sequenced: CMPLXF's two arguments are
           indeterminately sequenced too. gcc and clang happen to
           agree here (real takes the first draw); pinned anyway,
           because "they agree today" is not a guarantee. */
        float n_re = sigma * (float)dp_gauss (&st);
        float n_im = sigma * (float)dp_gauss (&st);
        burst[i] += CMPLXF (n_re, n_im);
      }
    /* Narrow loops: snr_est measures the EFFECTIVE post-loop SNR — the
     * tracking loops' residual phase jitter rotates signal energy into
     * Im, so the estimate sits below the AWGN-only value by the jitter
     * term A^2*sigma_phi^2 and converges to it as bn -> 0 (at bn = 0.05
     * the gap is ~6 dB; at 0.005, ~2 dB). That is the BER-relevant
     * quantity a consumer wants; the window below brackets it. */
    d = burst_despreader_create (code, sf, sf, sps, 0.0, 0.0, 0.005, 0.005);
    DP_CHECK (burst_despreader_get_stat_n (d) == 0);
    DP_CHECK (burst_despreader_get_lock_stat (d) == 0.0);
    (void)burst_despreader_bits (d, burst, blen, rx, nsym);
    DP_CHECK (burst_despreader_get_stat_n (d) == nsym);
    DP_CHECK (burst_despreader_get_lock_metric (d) > 0.85);
    /* AWGN-only post-despread per-component SNR = tsamps = sf*sps at
     * A = sigma = 1; the effective estimate lands under it by the
     * seed-dependent jitter term (bounds sized off a 200-seed sweep). */
    double snr_true = (double)(sf * sps);
    double snr_hat  = burst_despreader_get_snr_est (d);
    DP_CHECK (snr_hat > 0.3 * snr_true && snr_hat < 1.3 * snr_true);
    DP_CHECK (burst_despreader_get_lock_stat (d) > 30.0);
    burst_despreader_reset (d);
    DP_CHECK (burst_despreader_get_stat_n (d) == 0);
    DP_CHECK (burst_despreader_get_lock_stat (d) == 0.0);
    DP_CHECK (burst_despreader_get_snr_est (d) == 0.0);
    burst_despreader_destroy (d);
    free (burst);
  }

  free (tx);
  free (rx);
  /* serializable state — whole-struct (loop_filter children embedded); the
   * owned code pointers are preserved across set_state. */
  {
    uint8_t code[31];
    for (int i = 0; i < 31; i++)
      code[i] = (uint8_t)(i & 1);
    float complex rx[256], sym[8];
    for (int i = 0; i < 256; i++)
      rx[i] = (float)(i % 5) - 2.0f + 0.2f * I;
    burst_despreader_state_t *a
        = burst_despreader_create (code, 31, 31, 4, 0.0, 0.0, 0.05, 0.01);
    burst_despreader_state_t *b
        = burst_despreader_create (code, 31, 31, 4, 0.0, 0.0, 0.05, 0.01);
    DP_CHECK (a != NULL && b != NULL);
    (void)burst_despreader_steps (a, rx, 256, sym, 8);
    DP_STATE_ROUNDTRIP_TEST (burst_despreader, a, b);
    DP_CHECK (b->car_phase == a->car_phase && b->acc_p == a->acc_p);
    DP_CHECK (b->code != NULL && b->code != a->code);
    burst_despreader_destroy (a);
    burst_despreader_destroy (b);
  }

  /* ── lock_metric's two documented constants ───────────────────────────
   *
   * The header states both ends: ~1 when phase-locked, and ~2/pi = 0.6366
   * with no carrier, because the metric is the mean of |cos theta| over a
   * uniform theta. Only the locked end was pinned (`> 0.9`), so a metric
   * that had stopped responding to the carrier at all would have to fall
   * below 0.9 before anything noticed -- and 2/pi is 0.64, which is not
   * far below it.
   *
   * Pinning the unlocked value is what makes the locked one mean
   * something: the two must be SEPARATED, not merely both plausible. */
  {
    const size_t sfl = 31, spsl = 2, nsyml = 64;
    uint8_t      c31[31];
    for (size_t i = 0; i < 31; i++)
      c31[i] = (uint8_t)(i & 1u);

    /* Noise only: no carrier to lock to, so |Re P|/|P| averages |cos| of a
       uniform phase = 2/pi. */
    burst_despreader_state_t *d
        = burst_despreader_create (c31, sfl, sfl, spsl, 0.0, 0.0, 0.05, 0.01);
    DP_CHECK (d != NULL);
    if (d)
      {
        size_t         n = nsyml * sfl * spsl;
        float complex *x = malloc (n * sizeof *x);
        DP_CHECK (x != NULL);
        if (x)
          {
            uint32_t st = 20260825u;
            for (size_t k = 0; k < n; k++)
              {
                /* Named locals: two dp_gauss draws in one expression are
                   indeterminately sequenced. */
                double re = dp_gauss (&st);
                double im = dp_gauss (&st);
                x[k]      = (float)re + (float)im * I;
              }
            float complex out[64];
            for (size_t off = 0; off + 64 <= n; off += 64)
              (void)burst_despreader_steps (d, x + off, 64, out, 64);
            double lm = burst_despreader_get_lock_metric (d);
            DP_CHECK (lm > 0.55 && lm < 0.72); /* 2/pi = 0.6366 */
            free (x);
          }
        burst_despreader_destroy (d);
      }
  }

  /* ── lock_stat returns 0 for TWO different reasons ────────────────────
   *
   * Documented: 0 before any payload prompt. Undocumented until this
   * certification: also 0 when the quadrature sum is exactly zero, because
   * the ratio is undefined there. That is a perfectly noiseless input --
   * which never happens on the air and happens constantly in tests, so the
   * WORST reading of the statistic is what a synthetic clean burst
   * produces. The header's own example asserted the opposite and was
   * corrected alongside this test.
   *
   * Both cases are pinned, and so is the discriminator: stat_n. */
  {
    const size_t sfl = 31, spsl = 2, nsyml = 32;
    uint8_t      c31[31];
    for (size_t i = 0; i < 31; i++)
      c31[i] = (uint8_t)(i & 1u);

    burst_despreader_state_t *d
        = burst_despreader_create (c31, sfl, sfl, spsl, 0.0, 0.0, 0.05, 0.01);
    DP_CHECK (d != NULL);
    if (d)
      {
        /* (a) nothing fed yet */
        DP_CHECK (burst_despreader_get_stat_n (d) == 0);
        DP_CHECK (burst_despreader_get_lock_stat (d) == 0.0);

        size_t         n = nsyml * sfl * spsl;
        float complex *x = malloc (n * sizeof *x);
        DP_CHECK (x != NULL);
        if (x)
          {
            for (size_t k = 0; k < n; k++)
              {
                uint8_t chip = c31[(k / spsl) % sfl];
                x[k]         = (chip & 1u) ? -1.0f : 1.0f; /* Im exactly 0 */
              }
            float complex out[64];
            for (size_t off = 0; off + 64 <= n; off += 64)
              (void)burst_despreader_steps (d, x + off, 64, out, 64);
            /* (b) payload folded, but the quadrature sum is exactly zero */
            DP_CHECK (burst_despreader_get_stat_n (d) > 0);
            DP_CHECK (burst_despreader_get_lock_stat (d) == 0.0);
            free (x);
          }
        burst_despreader_destroy (d);
      }
  }

  /* ── set_acq excludes the preamble from the burst statistics ──────────
   *
   * The header is explicit that only payload prompts fold in, "so the H0
   * law and the SNR calibration hold" -- preamble prompts use a different
   * code length and sit inside the pull-in transient. set_acq was called
   * twice in this file with NO assertions at all, so the exclusion was
   * pinned by nothing.
   *
   * The discriminating check is the pair: with the preamble declared,
   * stat_n counts exactly the payload symbols; feeding the same stream
   * WITHOUT declaring it leaves an extra prompt folded in. */
  {
    const size_t sfl = 31, spsl = 2, nsyml = 40, asf = 16, areps = 3;
    uint8_t      c31[31], acq[16];
    for (size_t i = 0; i < 31; i++)
      c31[i] = (uint8_t)(i & 1u);
    for (size_t i = 0; i < 16; i++)
      acq[i] = (uint8_t)(i & 1u);

    const size_t   pre_n = areps * asf * spsl;
    const size_t   pay_n = nsyml * sfl * spsl;
    float complex *x     = malloc ((pre_n + pay_n) * sizeof *x);
    DP_CHECK (x != NULL);
    if (x)
      {
        for (size_t k = 0; k < pre_n; k++)
          {
            uint8_t chip = acq[(k / spsl) % asf];
            x[k]         = (chip & 1u) ? -1.0f : 1.0f;
          }
        for (size_t k = 0; k < pay_n; k++)
          {
            uint8_t chip = c31[(k / spsl) % sfl];
            x[pre_n + k] = (chip & 1u) ? -1.0f : 1.0f;
          }
        float complex out[64];

        burst_despreader_state_t *a = burst_despreader_create (
            c31, sfl, sfl, spsl, 0.0, 0.0, 0.05, 0.01);
        DP_CHECK (a != NULL);
        if (a)
          {
            burst_despreader_set_acq (a, acq, asf, areps);
            for (size_t off = 0; off + 64 <= pre_n + pay_n; off += 64)
              (void)burst_despreader_steps (a, x + off, 64, out, 64);
            size_t with_acq = burst_despreader_get_stat_n (a);

            burst_despreader_state_t *b = burst_despreader_create (
                c31, sfl, sfl, spsl, 0.0, 0.0, 0.05, 0.01);
            DP_CHECK (b != NULL);
            if (b)
              {
                for (size_t off = 0; off + 64 <= pre_n + pay_n; off += 64)
                  (void)burst_despreader_steps (b, x + off, 64, out, 64);
                size_t without = burst_despreader_get_stat_n (b);
                /* Declaring the preamble EXCLUDES it; not declaring it
                   folds the preamble's prompts in. The inequality is the
                   whole claim -- an equal count would mean the preamble
                   was never excluded. */
                DP_CHECK (with_acq < without);
                burst_despreader_destroy (b);
              }
            burst_despreader_destroy (a);
          }
        free (x);
      }
  }

  DP_TEST_END ("test_burst_despreader_core");
}
