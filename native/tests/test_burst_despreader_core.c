#include "burst_despreader/burst_despreader_core.h"
#include "dp_state_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

/* xorshift + Box-Muller unit-variance Gaussian (per component). */
static float
gauss2 (uint32_t *st)
{
  uint32_t a  = (*st ^= *st << 13, *st ^= *st >> 17, *st ^= *st << 5, *st);
  uint32_t b  = (*st ^= *st << 13, *st ^= *st >> 17, *st ^= *st << 5, *st);
  double   r1 = ((double)a + 1.0) / 4294967297.0;
  double   r2 = ((double)b + 1.0) / 4294967297.0;
  return (float)(sqrt (-2.0 * log (r1)) * cos (2.0 * M_PI * r2));
}

int
main (void)
{
  int _fails = 0;

  /* Invalid args -> NULL (not a silent zero state). */
  CHECK (burst_despreader_create (NULL, 0, 1, 2, 0.0, 0.0, 0.05, 0.01)
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
  CHECK (d != NULL);
  size_t n_out = burst_despreader_bits (d, burst, blen, rx, nsym);
  CHECK (n_out == nsym);
  CHECK (amb_ber (rx, tx, 0, n_out) == 0.0);
  burst_despreader_destroy (d);
  free (burst);

  /* (2) Carrier offset, seeded at the true frequency -> exact recovery,
   *     loop holds the frequency. */
  double f0 = 0.0006;
  burst     = make_burst (code, sf, sps, nsym, f0, tx, &blen);
  d         = burst_despreader_create (code, sf, sf, sps, f0, 0.0, 0.05, 0.01);
  n_out     = burst_despreader_bits (d, burst, blen, rx, nsym);
  CHECK (amb_ber (rx, tx, n_out / 4, n_out) == 0.0);
  CHECK (fabs (burst_despreader_get_norm_freq (d) - f0) < 1e-4);
  CHECK (burst_despreader_get_lock_metric (d) > 0.9);

  /* (3) reset re-seeds; a second identical run reproduces the first. */
  burst_despreader_reset (d);
  uint8_t *rx2 = malloc (nsym);
  size_t   n2  = burst_despreader_bits (d, burst, blen, rx2, nsym);
  CHECK (n2 == n_out);
  CHECK (amb_ber (rx2, tx, n2 / 4, n2) == 0.0);

  /* (4) property accessors round-trip. */
  burst_despreader_set_bn_carrier (d, 0.06);
  CHECK (burst_despreader_get_bn_carrier (d) == 0.06);
  burst_despreader_set_bn_code (d, 0.02);
  CHECK (burst_despreader_get_bn_code (d) == 0.02);
  burst_despreader_set_norm_freq (d, 0.001);
  CHECK (fabs (burst_despreader_get_norm_freq (d) - 0.001) < 1e-9);
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
      burst[i] += CMPLXF (sigma * gauss2 (&st), sigma * gauss2 (&st));
    /* Narrow loops: snr_est measures the EFFECTIVE post-loop SNR — the
     * tracking loops' residual phase jitter rotates signal energy into
     * Im, so the estimate sits below the AWGN-only value by the jitter
     * term A^2*sigma_phi^2 and converges to it as bn -> 0 (at bn = 0.05
     * the gap is ~6 dB; at 0.005, ~2 dB). That is the BER-relevant
     * quantity a consumer wants; the window below brackets it. */
    d = burst_despreader_create (code, sf, sf, sps, 0.0, 0.0, 0.005, 0.005);
    CHECK (burst_despreader_get_stat_n (d) == 0);
    CHECK (burst_despreader_get_lock_stat (d) == 0.0);
    (void)burst_despreader_bits (d, burst, blen, rx, nsym);
    CHECK (burst_despreader_get_stat_n (d) == nsym);
    CHECK (burst_despreader_get_lock_metric (d) > 0.85);
    /* AWGN-only post-despread per-component SNR = tsamps = sf*sps at
     * A = sigma = 1; the effective estimate lands under it by the
     * seed-dependent jitter term (bounds sized off a 200-seed sweep). */
    double snr_true = (double)(sf * sps);
    double snr_hat  = burst_despreader_get_snr_est (d);
    CHECK (snr_hat > 0.3 * snr_true && snr_hat < 1.3 * snr_true);
    CHECK (burst_despreader_get_lock_stat (d) > 30.0);
    burst_despreader_reset (d);
    CHECK (burst_despreader_get_stat_n (d) == 0);
    CHECK (burst_despreader_get_lock_stat (d) == 0.0);
    CHECK (burst_despreader_get_snr_est (d) == 0.0);
    burst_despreader_destroy (d);
    free (burst);
  }

  free (tx);
  free (rx);
  if (_fails)
    {
      fprintf (stderr, "test_burst_despreader_core FAILED (%d)\n", _fails);
      return 1;
    }
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
    CHECK (a != NULL && b != NULL);
    (void)burst_despreader_steps (a, rx, 256, sym, 8);
    DP_STATE_ROUNDTRIP_TEST (burst_despreader, a, b);
    CHECK (b->car_phase == a->car_phase && b->acc_p == a->acc_p);
    CHECK (b->code != NULL && b->code != a->code);
    burst_despreader_destroy (a);
    burst_despreader_destroy (b);
  }

  printf ("test_burst_despreader_core PASSED\n");
  return 0;
}
