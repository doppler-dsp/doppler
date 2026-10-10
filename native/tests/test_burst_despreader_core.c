#include "doppler/burst_despreader/burst_despreader_core.h"
#include "doppler/dp_complex.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Spread `nsym` BPSK bits by `code` (length sf), oversample by sps
 * (rectangular hold), and optionally rotate by a per-sample carrier `f0`
 * (cycles/sample). Returns a malloc'd cf32 burst of nsym*sf*sps samples; fills
 * tx_bits. */
static float _Complex *
make_burst (const uint8_t *code, size_t sf, size_t sps, size_t nsym, double f0,
            uint8_t *tx_bits, size_t *out_len)
{
  size_t          nsamp = nsym * sf * sps;
  float _Complex *x     = malloc (nsamp * sizeof (*x));
  size_t          k     = 0;
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
              float _Complex c
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

/* The bytes an object's state is: two objects whose blobs compare equal are
   in the same state, and a refused set_state must leave them so. */
static int
bd_same_state (const dp_burst_despreader_state_t *a,
               const dp_burst_despreader_state_t *b)
{
  size_t na = dp_burst_despreader_state_bytes (a);
  if (na != dp_burst_despreader_state_bytes (b))
    return 0;
  unsigned char *ba = malloc (na), *bb = malloc (na);
  int            same = 0;
  if (ba && bb)
    {
      dp_burst_despreader_get_state (a, ba);
      dp_burst_despreader_get_state (b, bb);
      same = memcmp (ba, bb, na) == 0;
    }
  free (ba);
  free (bb);
  return same;
}

/* set_state refuses blob, and leaves target as twin, an untouched copy.
   A target the blob did change is put back from twin, so each check stands
   on its own rather than failing for an earlier one's sake. */
static int
bd_refused (dp_burst_despreader_state_t       *target,
            const dp_burst_despreader_state_t *twin, const void *blob)
{
  int refused = dp_burst_despreader_set_state (target, blob) == DP_ERR_INVALID;
  int same    = bd_same_state (target, twin);
  if (!same)
    {
      unsigned char *b = malloc (dp_burst_despreader_state_bytes (twin));
      if (b)
        {
          dp_burst_despreader_get_state (twin, b);
          (void)dp_burst_despreader_set_state (target, b);
          free (b);
        }
    }
  return refused && same;
}

/* bd_refused for a copy of good with one struct field overwritten. */
static int
bd_forged_refused (dp_burst_despreader_state_t       *target,
                   const dp_burst_despreader_state_t *twin,
                   const unsigned char *good, size_t n, size_t off,
                   const void *v, size_t vn)
{
  unsigned char *bad = malloc (n);
  if (!bad)
    return 0;
  memcpy (bad, good, n);
  memcpy (bad + sizeof (dp_state_hdr_t) + off, v, vn);
  int refused = bd_refused (target, twin, bad);
  free (bad);
  return refused;
}

int
main (void)
{

  /* Invalid args -> NULL (not a silent zero state). */
  DP_CHECK (dp_burst_despreader_create (NULL, 0, 1, 2, 0.0, 0.0, 0.05, 0.01)
            == NULL);

  size_t  sf = 31, sps = 4, nsym = 120;
  uint8_t code[31];
  for (size_t i = 0; i < sf; i++)
    code[i] = (uint8_t)((i * 2246822519u) >> 31) & 1u;

  /* A bandwidth outside the loop filter's domain (finite, >= 0) is refused
     at create and by both setters, which keep the loop's bandwidth: a NaN
     or infinite bn gave NaN gains from the first symbol (#2071). */
  {
    const double bad[] = { NAN, INFINITY, -0.01 };
    for (size_t i = 0; i < 3; i++)
      {
        DP_CHECK (dp_burst_despreader_create (code, sf, sf, sps, 0.0, 0.0,
                                              bad[i], 0.01)
                  == NULL);
        DP_CHECK (dp_burst_despreader_create (code, sf, sf, sps, 0.0, 0.0,
                                              0.05, bad[i])
                  == NULL);
      }
    dp_burst_despreader_state_t *d
        = dp_burst_despreader_create (code, sf, sf, sps, 0.0, 0.0, 0.05, 0.01);
    DP_REQUIRE (d != NULL);
    for (size_t i = 0; i < 3; i++)
      {
        DP_CHECK (dp_burst_despreader_set_bn_carrier (d, bad[i])
                  == DP_ERR_INVALID);
        DP_CHECK (dp_burst_despreader_set_bn_code (d, bad[i])
                  == DP_ERR_INVALID);
      }
    DP_CHECK (dp_burst_despreader_get_bn_carrier (d) == 0.05);
    DP_CHECK (dp_burst_despreader_get_bn_code (d) == 0.01);
    /* The accepted side: 0 (a frozen loop) and an ordinary value. */
    DP_CHECK (dp_burst_despreader_set_bn_carrier (d, 0.0) == DP_OK);
    DP_CHECK (dp_burst_despreader_set_bn_code (d, 0.02) == DP_OK);
    DP_CHECK (dp_burst_despreader_get_bn_code (d) == 0.02);
    dp_burst_despreader_destroy (d);
  }

  uint8_t *tx = malloc (nsym), *rx = malloc (nsym);
  size_t   blen = 0;

  /* (1) Genie: zero offset, no noise -> exact recovery. */
  float _Complex *burst = make_burst (code, sf, sps, nsym, 0.0, tx, &blen);
  dp_burst_despreader_state_t *d
      = dp_burst_despreader_create (code, sf, sf, sps, 0.0, 0.0, 0.05, 0.01);
  DP_CHECK (d != NULL);
  size_t n_out = dp_burst_despreader_bits (d, burst, blen, rx, nsym);
  DP_CHECK (n_out == nsym);
  DP_CHECK (amb_ber (rx, tx, 0, n_out) == 0.0);
  dp_burst_despreader_destroy (d);
  free (burst);

  /* (2) Carrier offset, seeded at the true frequency -> exact recovery,
   *     loop holds the frequency. */
  double f0 = 0.0006;
  burst     = make_burst (code, sf, sps, nsym, f0, tx, &blen);
  d     = dp_burst_despreader_create (code, sf, sf, sps, f0, 0.0, 0.05, 0.01);
  n_out = dp_burst_despreader_bits (d, burst, blen, rx, nsym);
  DP_CHECK (amb_ber (rx, tx, n_out / 4, n_out) == 0.0);
  DP_CHECK (fabs (dp_burst_despreader_get_norm_freq (d) - f0) < 1e-4);
  DP_CHECK (dp_burst_despreader_get_lock_metric (d) > 0.9);

  /* (3) reset re-seeds; a second identical run reproduces the first. */
  dp_burst_despreader_reset (d);
  uint8_t *rx2 = malloc (nsym);
  size_t   n2  = dp_burst_despreader_bits (d, burst, blen, rx2, nsym);
  DP_CHECK (n2 == n_out);
  DP_CHECK (amb_ber (rx2, tx, n2 / 4, n2) == 0.0);

  /* (4) property accessors round-trip. */
  dp_burst_despreader_set_bn_carrier (d, 0.06);
  DP_CHECK (dp_burst_despreader_get_bn_carrier (d) == 0.06);
  dp_burst_despreader_set_bn_code (d, 0.02);
  DP_CHECK (dp_burst_despreader_get_bn_code (d) == 0.02);
  dp_burst_despreader_set_norm_freq (d, 0.001);
  DP_CHECK (fabs (dp_burst_despreader_get_norm_freq (d) - 0.001) < 1e-9);
  (void)dp_burst_despreader_get_code_phase (d);
  (void)dp_burst_despreader_get_lock_metric (d);
  (void)dp_burst_despreader_get_snr_est (d);

  /* (5) set_acq enable then disable (payload-only). */
  uint8_t acq[16];
  for (size_t i = 0; i < 16; i++)
    acq[i] = (uint8_t)(i & 1u);
  dp_burst_despreader_set_acq (d, acq, 16, 3);
  dp_burst_despreader_set_acq (d, NULL, 0, 0); /* disable */

  dp_burst_despreader_destroy (d);
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
    d = dp_burst_despreader_create (code, sf, sf, sps, 0.0, 0.0, 0.005, 0.005);
    DP_CHECK (dp_burst_despreader_get_stat_n (d) == 0);
    DP_CHECK (dp_burst_despreader_get_lock_stat (d) == 0.0);
    (void)dp_burst_despreader_bits (d, burst, blen, rx, nsym);
    DP_CHECK (dp_burst_despreader_get_stat_n (d) == nsym);
    DP_CHECK (dp_burst_despreader_get_lock_metric (d) > 0.85);
    /* AWGN-only post-despread per-component SNR = tsamps = sf*sps at
     * A = sigma = 1; the effective estimate lands under it by the
     * seed-dependent jitter term (bounds sized off a 200-seed sweep). */
    double snr_true = (double)(sf * sps);
    double snr_hat  = dp_burst_despreader_get_snr_est (d);
    DP_CHECK (snr_hat > 0.3 * snr_true && snr_hat < 1.3 * snr_true);
    DP_CHECK (dp_burst_despreader_get_lock_stat (d) > 30.0);
    dp_burst_despreader_reset (d);
    DP_CHECK (dp_burst_despreader_get_stat_n (d) == 0);
    DP_CHECK (dp_burst_despreader_get_lock_stat (d) == 0.0);
    DP_CHECK (dp_burst_despreader_get_snr_est (d) == 0.0);
    dp_burst_despreader_destroy (d);
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
    float _Complex rx[256], sym[8];
    for (int i = 0; i < 256; i++)
      rx[i] = (float)(i % 5) - 2.0f + 0.2f * I;
    dp_burst_despreader_state_t *a
        = dp_burst_despreader_create (code, 31, 31, 4, 0.0, 0.0, 0.05, 0.01);
    dp_burst_despreader_state_t *b
        = dp_burst_despreader_create (code, 31, 31, 4, 0.0, 0.0, 0.05, 0.01);
    DP_CHECK (a != NULL && b != NULL);
    (void)dp_burst_despreader_steps (a, rx, 256, sym, 8);
    DP_STATE_ROUNDTRIP_TEST (dp_burst_despreader, a, b);
    DP_CHECK (b->car_phase == a->car_phase && b->acc_p == a->acc_p);
    DP_CHECK (b->code != NULL && b->code != a->code);
    dp_burst_despreader_destroy (a);
    dp_burst_despreader_destroy (b);
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
    dp_burst_despreader_state_t *d = dp_burst_despreader_create (
        c31, sfl, sfl, spsl, 0.0, 0.0, 0.05, 0.01);
    DP_CHECK (d != NULL);
    if (d)
      {
        size_t          n = nsyml * sfl * spsl;
        float _Complex *x = malloc (n * sizeof *x);
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
            float _Complex out[64];
            for (size_t off = 0; off + 64 <= n; off += 64)
              (void)dp_burst_despreader_steps (d, x + off, 64, out, 64);
            double lm = dp_burst_despreader_get_lock_metric (d);
            DP_CHECK (lm > 0.55 && lm < 0.72); /* 2/pi = 0.6366 */
            free (x);
          }
        dp_burst_despreader_destroy (d);
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

    dp_burst_despreader_state_t *d = dp_burst_despreader_create (
        c31, sfl, sfl, spsl, 0.0, 0.0, 0.05, 0.01);
    DP_CHECK (d != NULL);
    if (d)
      {
        /* (a) nothing fed yet */
        DP_CHECK (dp_burst_despreader_get_stat_n (d) == 0);
        DP_CHECK (dp_burst_despreader_get_lock_stat (d) == 0.0);

        size_t          n = nsyml * sfl * spsl;
        float _Complex *x = malloc (n * sizeof *x);
        DP_CHECK (x != NULL);
        if (x)
          {
            for (size_t k = 0; k < n; k++)
              {
                uint8_t chip = c31[(k / spsl) % sfl];
                x[k]         = (chip & 1u) ? -1.0f : 1.0f; /* Im exactly 0 */
              }
            float _Complex out[64];
            for (size_t off = 0; off + 64 <= n; off += 64)
              (void)dp_burst_despreader_steps (d, x + off, 64, out, 64);
            /* (b) payload folded, but the quadrature sum is exactly zero */
            DP_CHECK (dp_burst_despreader_get_stat_n (d) > 0);
            DP_CHECK (dp_burst_despreader_get_lock_stat (d) == 0.0);
            free (x);
          }
        dp_burst_despreader_destroy (d);
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

    const size_t    pre_n = areps * asf * spsl;
    const size_t    pay_n = nsyml * sfl * spsl;
    float _Complex *x     = malloc ((pre_n + pay_n) * sizeof *x);
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
        float _Complex out[64];

        dp_burst_despreader_state_t *a = dp_burst_despreader_create (
            c31, sfl, sfl, spsl, 0.0, 0.0, 0.05, 0.01);
        DP_CHECK (a != NULL);
        if (a)
          {
            dp_burst_despreader_set_acq (a, acq, asf, areps);
            for (size_t off = 0; off + 64 <= pre_n + pay_n; off += 64)
              (void)dp_burst_despreader_steps (a, x + off, 64, out, 64);
            size_t with_acq = dp_burst_despreader_get_stat_n (a);

            dp_burst_despreader_state_t *b = dp_burst_despreader_create (
                c31, sfl, sfl, spsl, 0.0, 0.0, 0.05, 0.01);
            DP_CHECK (b != NULL);
            if (b)
              {
                for (size_t off = 0; off + 64 <= pre_n + pay_n; off += 64)
                  (void)dp_burst_despreader_steps (b, x + off, 64, out, 64);
                size_t without = dp_burst_despreader_get_stat_n (b);
                /* Declaring the preamble EXCLUDES it; not declaring it
                   folds the preamble's prompts in. The inequality is the
                   whole claim -- an equal count would mean the preamble
                   was never excluded. */
                DP_CHECK (with_acq < without);
                dp_burst_despreader_destroy (b);
              }
            dp_burst_despreader_destroy (a);
          }
        free (x);
      }
  }

  /* ── set_state reads nothing it cannot index, and a refusal writes
   *    nothing (#2041) ─────────────────────────────────────────────────
   *
   * set_state read the blob straight into the live object, sf and the
   * acq fields included, while keeping this object's own code buffers:
   * a blob from a despreader with set_acq() active, restored into one
   * without, returned DP_OK and the next steps() read a NULL acq code. */
  {
    uint8_t code[31], acq[127], acq2[127];
    for (int i = 0; i < 31; i++)
      code[i] = (uint8_t)((i * 7 + 3) % 2);
    for (int i = 0; i < 127; i++)
      {
        acq[i]  = (uint8_t)((i * 5 + 1) % 2);
        acq2[i] = (uint8_t)((i * 3) % 2);
      }
    enum
    {
      NX = 4096
    };
    float _Complex *x  = malloc (NX * sizeof *x);
    float _Complex *oa = malloc (NX * sizeof *oa);
    float _Complex *ob = malloc (NX * sizeof *ob);
    DP_REQUIRE (x && oa && ob);
    for (int i = 0; i < NX; i++)
      x[i] = (float)(i % 5) - 2.0f + 0.2f * (float)(i % 3) * I;

    dp_burst_despreader_state_t *a
        = dp_burst_despreader_create (code, 31, 31, 4, 0.0, 0.0, 0.01, 0.01);
    dp_burst_despreader_state_t *b
        = dp_burst_despreader_create (code, 31, 31, 4, 0.0, 0.0, 0.01, 0.01);
    dp_burst_despreader_state_t *twin
        = dp_burst_despreader_create (code, 31, 31, 4, 0.0, 0.0, 0.01, 0.01);
    DP_REQUIRE (a && b && twin);
    dp_burst_despreader_set_acq (a, acq, 127, 4);
    (void)dp_burst_despreader_steps (a, x, 600, oa, NX); /* mid-preamble */
    DP_REQUIRE (a->preamble_left > 0 && a->preamble_left < 4);
    unsigned char *blob = malloc (dp_burst_despreader_state_bytes (a));
    DP_REQUIRE (blob != NULL);
    dp_burst_despreader_get_state (a, blob);

    /* The issue's repro: refused, untouched, and steps() runs. */
    DP_CHECK (dp_burst_despreader_set_state (b, blob) == DP_ERR_INVALID);
    DP_CHECK (bd_same_state (b, twin));
    (void)dp_burst_despreader_steps (b, x, 64, ob, NX);

    /* Another code length or chip rate: refused, untouched. The blob of a
       despreader with no acq code has this one's size, so only the key
       can refuse it. */
    {
      dp_burst_despreader_state_t *src
          = dp_burst_despreader_create (code, 31, 31, 4, 0.0, 0.0, 0.01, 0.01);
      dp_burst_despreader_state_t *sf15
          = dp_burst_despreader_create (code, 31, 15, 4, 0.0, 0.0, 0.01, 0.01);
      dp_burst_despreader_state_t *sf15t
          = dp_burst_despreader_create (code, 31, 15, 4, 0.0, 0.0, 0.01, 0.01);
      dp_burst_despreader_state_t *sps2
          = dp_burst_despreader_create (code, 31, 31, 2, 0.0, 0.0, 0.01, 0.01);
      dp_burst_despreader_state_t *sps2t
          = dp_burst_despreader_create (code, 31, 31, 2, 0.0, 0.0, 0.01, 0.01);
      DP_REQUIRE (src && sf15 && sf15t && sps2 && sps2t);
      (void)dp_burst_despreader_steps (src, x, 300, ob, NX);
      unsigned char *b2 = malloc (dp_burst_despreader_state_bytes (src));
      DP_REQUIRE (b2 != NULL);
      dp_burst_despreader_get_state (src, b2);
      DP_CHECK (dp_burst_despreader_state_bytes (sf15)
                == dp_burst_despreader_state_bytes (src));
      DP_CHECK (dp_burst_despreader_set_state (sf15, b2) == DP_ERR_INVALID);
      DP_CHECK (bd_same_state (sf15, sf15t));
      DP_CHECK (dp_burst_despreader_set_state (sps2, b2) == DP_ERR_INVALID);
      DP_CHECK (bd_same_state (sps2, sps2t));
      free (b2);
      dp_burst_despreader_destroy (src);
      dp_burst_despreader_destroy (sf15);
      dp_burst_despreader_destroy (sf15t);
      dp_burst_despreader_destroy (sps2);
      dp_burst_despreader_destroy (sps2t);
    }

    /* The setter's value travels: a despreader whose acq code is the same
       LENGTH but other chips, tracked for another number of periods,
       restores to a's preamble and from there runs exactly as a does. */
    dp_burst_despreader_state_t *c
        = dp_burst_despreader_create (code, 31, 31, 4, 0.0, 0.0, 0.01, 0.01);
    dp_burst_despreader_state_t *ct
        = dp_burst_despreader_create (code, 31, 31, 4, 0.0, 0.0, 0.01, 0.01);
    DP_REQUIRE (c && ct);
    dp_burst_despreader_set_acq (c, acq2, 127, 2);
    dp_burst_despreader_set_acq (ct, acq2, 127, 2);
    DP_CHECK (dp_burst_despreader_set_state (c, blob) == DP_OK);
    DP_CHECK (c->acq_reps == 4 && memcmp (c->acq_code, acq, 127) == 0);
    {
      size_t na = dp_burst_despreader_steps (a, x + 600, NX - 600, oa, NX);
      size_t nb = dp_burst_despreader_steps (c, x + 600, NX - 600, ob, NX);
      DP_CHECK (na > 0 && na == nb && memcmp (oa, ob, na * sizeof *oa) == 0);
    }

    /* Blobs no run could leave, each refused with the target untouched:
       more preamble left than periods, a preamble with no acq code, and a
       chip position that is not finite. */
    {
      const size_t   hdr = sizeof (dp_state_hdr_t);
      const size_t   nb  = dp_burst_despreader_state_bytes (ct);
      unsigned char *bad = malloc (nb), *good = malloc (nb);
      DP_REQUIRE (bad && good);
      dp_burst_despreader_get_state (ct, good);

      size_t too_many = 3; /* ct tracks 2 periods */
      memcpy (bad, good, nb);
      memcpy (bad + hdr
                  + offsetof (dp_burst_despreader_state_t, preamble_left),
              &too_many, sizeof too_many);
      DP_CHECK (dp_burst_despreader_set_state (c, bad) == DP_ERR_INVALID);

      static const double chips[] = { NAN, INFINITY, -INFINITY };
      for (size_t k = 0; k < sizeof chips / sizeof *chips; k++)
        {
          memcpy (bad, good, nb);
          memcpy (bad + hdr + offsetof (dp_burst_despreader_state_t, chip_pos),
                  &chips[k], sizeof chips[k]);
          DP_CHECK (dp_burst_despreader_set_state (c, bad) == DP_ERR_INVALID);
        }
      /* and c was not touched by any of them: it still equals a, whose
         state it restored and whose input it then followed */
      DP_CHECK (bd_same_state (c, a));
      free (bad);
      free (good);

      /* A blob of a despreader with NO acq code but preamble_left set: the
         size matches a no-acq target, so only the predicate refuses it. */
      const size_t   n0 = dp_burst_despreader_state_bytes (twin);
      unsigned char *b0 = malloc (n0), *pre = malloc (n0);
      size_t         one = 1;
      DP_REQUIRE (b0 && pre);
      dp_burst_despreader_get_state (twin, b0);
      memcpy (b0 + hdr + offsetof (dp_burst_despreader_state_t, preamble_left),
              &one, sizeof one);
      dp_burst_despreader_get_state (b, pre);
      DP_CHECK (dp_burst_despreader_set_state (b, b0) == DP_ERR_INVALID);
      dp_burst_despreader_get_state (b, b0); /* and b is as it was */
      DP_CHECK (memcmp (b0, pre, n0) == 0);
      free (b0);
      free (pre);
    }

    free (blob);
    free (x);
    free (oa);
    free (ob);
    dp_burst_despreader_destroy (a);
    dp_burst_despreader_destroy (b);
    dp_burst_despreader_destroy (twin);
    dp_burst_despreader_destroy (c);
    dp_burst_despreader_destroy (ct);
  }

  /* ── #2041, round 2: every config key, finite state, and a kernel whose
   *    chip index is total ───────────────────────────────────────────── */
  {
    uint8_t code[31], acq[127];
    for (int i = 0; i < 31; i++)
      code[i] = (uint8_t)((i * 7 + 3) % 2);
    for (int i = 0; i < 127; i++)
      acq[i] = (uint8_t)((i * 5 + 1) % 2);
    enum
    {
      NY = 2048
    };
    float _Complex *x  = malloc (NY * sizeof *x);
    float _Complex *oa = malloc (NY * sizeof *oa);
    float _Complex *ob = malloc (NY * sizeof *ob);
    DP_REQUIRE (x && oa && ob);
    for (int i = 0; i < NY; i++)
      x[i] = (float)(i % 5) - 2.0f + 0.2f * (float)(i % 3) * I;
#define BD_NEW(f0, c0)                                                        \
  dp_burst_despreader_create (code, 31, 31, 4, (f0), (c0), 0.01, 0.01)

    /* create refuses a seed that is not finite: set_state compares the
       seeds, and NaN equals nothing, its own blob included. */
    DP_CHECK (BD_NEW (NAN, 0.0) == NULL);
    DP_CHECK (BD_NEW (0.0, NAN) == NULL);
    DP_CHECK (BD_NEW (0.0, INFINITY) == NULL);

    dp_burst_despreader_state_t *t  = BD_NEW (0.0, 0.0);
    dp_burst_despreader_state_t *tt = BD_NEW (0.0, 0.0); /* t's twin */
    DP_REQUIRE (t && tt);
    const size_t   n    = dp_burst_despreader_state_bytes (t);
    unsigned char *good = malloc (n);
    DP_REQUIRE (good != NULL);
    dp_burst_despreader_get_state (tt, good);

    /* The seeds are create-time config reset() returns to: a blob of
       another seed, the same size as t's, is refused. */
    {
      dp_burst_despreader_state_t *f = BD_NEW (0.01, 0.0);
      dp_burst_despreader_state_t *c = BD_NEW (0.0, 0.5);
      DP_REQUIRE (f && c);
      unsigned char *bf = malloc (n), *bc = malloc (n);
      DP_REQUIRE (bf && bc);
      dp_burst_despreader_get_state (f, bf);
      dp_burst_despreader_get_state (c, bc);
      DP_CHECK_MSG (bd_refused (t, tt, bf), "another init_norm_freq");
      DP_CHECK_MSG (bd_refused (t, tt, bc), "another init_chip_phase");
      free (bf);
      free (bc);
      dp_burst_despreader_destroy (f);
      dp_burst_despreader_destroy (c);
    }

    /* bn is a setter's value, so it travels (#2022): the target takes the
       source's bandwidths and then runs exactly as the source does. */
    {
      dp_burst_despreader_state_t *src = BD_NEW (0.0, 0.0);
      dp_burst_despreader_state_t *dst = BD_NEW (0.0, 0.0);
      DP_REQUIRE (src && dst);
      dp_burst_despreader_set_bn_carrier (src, 0.02);
      dp_burst_despreader_set_bn_code (src, 0.005);
      (void)dp_burst_despreader_steps (src, x, 600, oa, NY);
      unsigned char *bs = malloc (n);
      DP_REQUIRE (bs != NULL);
      dp_burst_despreader_get_state (src, bs);
      DP_CHECK (dp_burst_despreader_set_state (dst, bs) == DP_OK);
      DP_CHECK (dp_burst_despreader_get_bn_carrier (dst) == 0.02
                && dp_burst_despreader_get_bn_code (dst) == 0.005);
      size_t na = dp_burst_despreader_steps (src, x + 600, NY - 600, oa, NY);
      size_t nb = dp_burst_despreader_steps (dst, x + 600, NY - 600, ob, NY);
      DP_CHECK (na > 0 && na == nb && memcmp (oa, ob, na * sizeof *oa) == 0);
      free (bs);
      dp_burst_despreader_destroy (src);
      dp_burst_despreader_destroy (dst);
    }

    /* Forged loop filters, each refused with t untouched: gains its bn
       does not derive, a damping or update period create never set, and
       numbers no healthy run leaves. */
    const size_t lc     = offsetof (dp_burst_despreader_state_t, lf_car);
    const size_t ld     = offsetof (dp_burst_despreader_state_t, lf_code);
    const double kp_off = 0.123, zeta = 0.5, two = 2.0, nan = NAN,
                 inf = INFINITY;
#define BD_FORGED(off, v, msg)                                                \
  DP_CHECK_MSG (bd_forged_refused (t, tt, good, n, (off), &(v), sizeof (v)),  \
                msg)
    BD_FORGED (lc + offsetof (dp_loop_filter_state_t, kp), kp_off,
               "a carrier kp its bn does not derive");
    BD_FORGED (ld + offsetof (dp_loop_filter_state_t, ki), kp_off,
               "a code ki its bn does not derive");
    BD_FORGED (ld + offsetof (dp_loop_filter_state_t, zeta), zeta,
               "another code-loop damping");
    BD_FORGED (lc + offsetof (dp_loop_filter_state_t, t), two,
               "another carrier update period");
    BD_FORGED (lc + offsetof (dp_loop_filter_state_t, bn), nan, "a NaN bn");
    BD_FORGED (ld + offsetof (dp_loop_filter_state_t, integ), nan,
               "a NaN code integrator");
    BD_FORGED (lc + offsetof (dp_loop_filter_state_t, integ), inf,
               "an infinite carrier integrator");

    /* Running state that is not finite. */
    BD_FORGED (offsetof (dp_burst_despreader_state_t, car_phase), nan,
               "a NaN carrier phase");
    BD_FORGED (offsetof (dp_burst_despreader_state_t, car_w), inf,
               "an infinite carrier rate");
    BD_FORGED (offsetof (dp_burst_despreader_state_t, chip_pos), nan,
               "a NaN chip position");
    BD_FORGED (offsetof (dp_burst_despreader_state_t, chip_pos), inf,
               "an infinite chip position");
    BD_FORGED (offsetof (dp_burst_despreader_state_t, code_rate), nan,
               "a NaN code rate");

    /* stat_n at SIZE_MAX would wrap to 0 and divide the lock metric by
       zero at the next payload prompt. */
    {
      const size_t top = SIZE_MAX;
      BD_FORGED (offsetof (dp_burst_despreader_state_t, stat_n), top,
                 "a stat_n that wraps");
    }

    /* acq_sf is a key on its own: a no-acq-size blob claiming a 127-chip
       preamble passes set_acq's predicate, and without the key set_state
       copied 127 bytes into this object's NULL acq code. */
    {
      unsigned char *bad = malloc (n);
      DP_REQUIRE (bad != NULL);
      const size_t acq_sf = 127, reps = 1, left = 1;
      memcpy (bad, good, n);
      memcpy (bad + sizeof (dp_state_hdr_t)
                  + offsetof (dp_burst_despreader_state_t, acq_sf),
              &acq_sf, sizeof acq_sf);
      memcpy (bad + sizeof (dp_state_hdr_t)
                  + offsetof (dp_burst_despreader_state_t, acq_reps),
              &reps, sizeof reps);
      memcpy (bad + sizeof (dp_state_hdr_t)
                  + offsetof (dp_burst_despreader_state_t, preamble_left),
              &left, sizeof left);
      DP_CHECK_MSG (bd_refused (t, tt, bad),
                    "an acq_sf the target has no acq code for");
      free (bad);
    }
#undef BD_FORGED

    /* No range bound on chip_pos: an object holds one past its acq-code
       length from set_acq() to its first boundary, and the blob it makes
       there restores and resumes bit-identically. */
    {
      dp_burst_despreader_state_t *src = BD_NEW (0.0, 20.0);
      dp_burst_despreader_state_t *dst = BD_NEW (0.0, 20.0);
      DP_REQUIRE (src && dst);
      dp_burst_despreader_set_acq (src, acq, 15, 2);
      dp_burst_despreader_set_acq (dst, acq, 15, 2);
      DP_REQUIRE (src->chip_pos > (double)src->acq_sf);
      unsigned char *bs = malloc (dp_burst_despreader_state_bytes (src));
      DP_REQUIRE (bs != NULL);
      dp_burst_despreader_get_state (src, bs);
      (void)dp_burst_despreader_steps (dst, x, 300, ob, NY); /* move dst */
      DP_CHECK (dp_burst_despreader_set_state (dst, bs) == DP_OK);
      size_t na = dp_burst_despreader_steps (src, x, NY, oa, NY);
      size_t nb = dp_burst_despreader_steps (dst, x, NY, ob, NY);
      DP_CHECK (na > 0 && na == nb && memcmp (oa, ob, na * sizeof *oa) == 0);
      free (bs);
      dp_burst_despreader_destroy (src);
      dp_burst_despreader_destroy (dst);
    }

    /* The kernel's chip index is total. A cast of a position at or below
       -1, past SIZE_MAX, or NaN is undefined, and each of these reached it:
       a create argument, a finite blob, and one NaN input sample. The gate
       is the UBSan leg (-fsanitize=float-cast-overflow); here they run,
       and the poisoned object's own blob is refused. */
    {
      dp_burst_despreader_state_t *neg = BD_NEW (0.0, -5.0);
      DP_REQUIRE (neg != NULL);
      DP_CHECK (dp_burst_despreader_steps (neg, x, NY, oa, NY) > 0);
      dp_burst_despreader_destroy (neg);

      dp_burst_despreader_state_t *far = BD_NEW (0.0, 0.0);
      DP_REQUIRE (far != NULL);
      unsigned char *bf = malloc (n);
      DP_REQUIRE (bf != NULL);
      const double big = 1e20;
      memcpy (bf, good, n);
      memcpy (bf + sizeof (dp_state_hdr_t)
                  + offsetof (dp_burst_despreader_state_t, chip_pos),
              &big, sizeof big);
      DP_CHECK (dp_burst_despreader_set_state (far, bf) == DP_OK);
      DP_CHECK (dp_burst_despreader_steps (far, x, NY, oa, NY) <= NY);
      dp_burst_despreader_destroy (far);

      dp_burst_despreader_state_t *pois = BD_NEW (0.0, 0.0);
      DP_REQUIRE (pois != NULL);
      memcpy (ob, x, NY * sizeof *x);
      ob[100] = NAN;
      DP_CHECK (dp_burst_despreader_steps (pois, ob, NY, oa, NY) <= NY);
      DP_CHECK (!isfinite (pois->code_rate) || !isfinite (pois->chip_pos));
      dp_burst_despreader_get_state (pois, bf);
      DP_CHECK_MSG (bd_refused (t, tt, bf),
                    "a NaN-poisoned object's own blob is refused");
      free (bf);
      dp_burst_despreader_destroy (pois);
    }
#undef BD_NEW

    free (good);
    free (x);
    free (oa);
    free (ob);
    dp_burst_despreader_destroy (t);
    dp_burst_despreader_destroy (tt);
  }

  DP_TEST_END ("test_burst_despreader_core");
}
