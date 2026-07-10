/**
 * @file test_despreader_core.c
 * @brief Unit tests for the continuous DSSS despreader (Costas + DLL).
 *
 * Tests:
 *   1. Lifecycle / NULL-code guard / init==create parity
 *   2. Full receiver — small residual: carrier + code lock, zero BER (steps)
 *   3. FLL assist — a residual the bare PLL misses is locked
 *   4. Hard bits (periods_per_bit = 1) match the data, up to a global flip
 *   5. Bit-sync (periods_per_bit > 1) recovers data bits + detects the
 * boundary
 *   6. Reset reproducibility
 */
#include "despreader/despreader_core.h"
#include "dp_state_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int
prbs (uint32_t *st)
{
  uint32_t x = *st;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *st = x;
  return (x & 1u) ? -1 : 1;
}

static void
make_code (uint8_t *code, size_t sf, uint32_t seed)
{
  uint32_t st = seed;
  for (size_t i = 0; i < sf; i++)
    code[i] = prbs (&st) > 0 ? 0u : 1u;
}

/* Build a continuous DSSS-BPSK signal: PN code x BPSK data (one data bit every
 * periods_per_bit code periods) x carrier exp(j 2pi f0 n), optional AWGN
 * sigma. Fills `data` with the +-1 bit per data symbol.  Returns sample count.
 */
static size_t
make_signal (float complex *rx, int *data, const uint8_t *code, size_t sf,
             size_t sps, size_t nper, size_t periods_per_bit, double f0,
             float sigma, uint32_t seed)
{
  uint32_t dst = seed ^ 0x5bd1e995u, nst = seed;
  size_t   k     = 0;
  double   cph   = 0.0;
  double   phase = 0.0, w = f0 * 2.0 * M_PI;
  int      bit = prbs (&dst);
  for (size_t p = 0; p < nper; p++)
    {
      if (p % periods_per_bit == 0) /* new data bit at the bit boundary */
        bit = prbs (&dst);
      data[p / periods_per_bit] = bit;
      for (size_t i = 0; i < sf * sps; i++, k++)
        {
          size_t        idx  = (size_t)fmod (cph, (double)sf);
          float         csgn = (code[idx] & 1u) ? -1.0f : 1.0f;
          float complex s    = (float)bit * csgn * cexpf ((float)phase * I);
          if (sigma > 0.0f)
            {
              float gr = 0, gi = 0;
              for (int j = 0; j < 4; j++)
                gr += (float)prbs (&nst);
              for (int j = 0; j < 4; j++)
                gi += (float)prbs (&nst);
              s += CMPLXF (sigma * gr * 0.5f, sigma * gi * 0.5f);
            }
          rx[k] = s;
          cph += 1.0 / (double)sps;
          phase += w;
        }
    }
  return k;
}

/* ambiguity-tolerant bit-error count of decisions vs truth over [lo,hi) */
static int
amb_errors (const int *dec, const int *truth, size_t lo, size_t hi)
{
  int err = 0, n = (int)(hi - lo);
  for (size_t i = lo; i < hi; i++)
    if (dec[i] != truth[i])
      err++;
  return err < n - err ? err : n - err;
}

int
main (void)
{
  int          _fails = 0;
  const size_t sf = 127, sps = 8, tsamps = sf * sps;

  /* 1. Lifecycle / guard / parity */
  {
    CHECK (despreader_create (NULL, 0, sps, 0.0, 0.0, 0.05, 0.005, 0.0, 0.707,
                              0.5, 1)
           == NULL);
    uint8_t code[127];
    make_code (code, sf, 1u);
    despreader_state_t *c = despreader_create (code, sf, sps, 0.001, 0.0, 0.05,
                                               0.005, 0.0, 0.707, 0.5, 1);
    CHECK (c != NULL);
    if (!c)
      return 1;
    CHECK (fabs (despreader_get_norm_freq (c) - 0.001) < 1e-9); /* seeded */
    CHECK (despreader_get_code_rate (c) == 1.0);
    despreader_state_t v;
    despreader_init (&v, code, sf, sps, 0.001, 0.0, 0.05, 0.005, 0.0, 0.707,
                     0.5, 1);
    CHECK (v.car.lf.kp == c->car.lf.kp);
    CHECK (v.code.sf == sf && v.code.owns_code == 0);
    free (v.flip_hist);
    despreader_destroy (c);
  }

  /* 2. Full receiver — small residual locks, zero BER on the tail */
  {
    const size_t nper = 500;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 7u);
    float complex *rx   = malloc (tsamps * nper * sizeof (*rx));
    int           *data = malloc (nper * sizeof (*data));
    size_t n = make_signal (rx, data, code, sf, sps, nper, 1, 5e-5, 0.0f, 3u);

    despreader_state_t *c   = despreader_create (code, sf, sps, 0.0, 0.0, 0.05,
                                                 0.005, 0.0, 0.707, 0.5, 1);
    float complex      *sym = malloc (nper * sizeof (*sym));
    size_t              k   = despreader_steps (c, rx, n, sym, nper);
    CHECK (fabs (despreader_get_norm_freq (c) - 5e-5) < 1e-5);
    CHECK (despreader_get_lock_metric (c) > 0.9);
    int *dec = malloc (k * sizeof (int));
    for (size_t i = 0; i < k; i++)
      dec[i] = (crealf (sym[i]) >= 0.0f) ? 1 : -1;
    CHECK (amb_errors (dec, data, k / 2, k) == 0);
    despreader_destroy (c);
    free (rx);
    free (data);
    free (sym);
    free (dec);
    free (code);
  }

  /* 3. FLL assist — a 0.2 cyc/epoch residual the bare PLL misses */
  {
    const size_t nper = 700;
    double       f0   = 0.2 / (double)tsamps; /* 0.2 cycles per code period */
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 9u);
    float complex *rx   = malloc (tsamps * nper * sizeof (*rx));
    int           *data = malloc (nper * sizeof (*data));
    size_t n = make_signal (rx, data, code, sf, sps, nper, 1, f0, 0.0f, 11u);

    despreader_state_t *pll = despreader_create (code, sf, sps, 0.0, 0.0, 0.05,
                                                 0.005, 0.0, 0.707, 0.5, 1);
    float complex      *sym = malloc (nper * sizeof (*sym));
    despreader_steps (pll, rx, n, sym, nper);
    CHECK (despreader_get_lock_metric (pll) < 0.8); /* bare PLL misses it */
    despreader_destroy (pll);

    despreader_state_t *fll = despreader_create (code, sf, sps, 0.0, 0.0, 0.05,
                                                 0.005, 0.03, 0.707, 0.5, 1);
    size_t              k   = despreader_steps (fll, rx, n, sym, nper);
    CHECK (fabs (despreader_get_norm_freq (fll) - f0) < 2e-5);
    CHECK (despreader_get_lock_metric (fll) > 0.9);
    int *dec = malloc (k * sizeof (int));
    for (size_t i = 0; i < k; i++)
      dec[i] = (crealf (sym[i]) >= 0.0f) ? 1 : -1;
    CHECK (amb_errors (dec, data, k / 2, k) == 0);
    despreader_destroy (fll);
    free (rx);
    free (data);
    free (sym);
    free (dec);
    free (code);
  }

  /* 4. Hard bits (periods_per_bit = 1) match the data */
  {
    const size_t nper = 400;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 13u);
    float complex *rx   = malloc (tsamps * nper * sizeof (*rx));
    int           *data = malloc (nper * sizeof (*data));
    size_t n = make_signal (rx, data, code, sf, sps, nper, 1, 4e-5, 0.0f, 17u);

    despreader_state_t *c = despreader_create (code, sf, sps, 0.0, 0.0, 0.05,
                                               0.005, 0.0, 0.707, 0.5, 1);
    uint8_t            *bits = malloc (nper);
    size_t              k    = despreader_bits (c, rx, n, bits, nper);
    int                *dec  = malloc (k * sizeof (int));
    for (size_t i = 0; i < k; i++)
      dec[i] = bits[i] ? 1 : -1;
    CHECK (amb_errors (dec, data, k / 2, k) == 0);
    despreader_destroy (c);
    free (rx);
    free (data);
    free (bits);
    free (dec);
    free (code);
  }

  /* 5. Bit-sync (periods_per_bit = 20) recovers data bits + detects the
   * boundary */
  {
    const size_t N = 20, nbits = 120, nper = N * nbits;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 19u);
    float complex *rx   = malloc (tsamps * nper * sizeof (*rx));
    int           *data = malloc (nbits * sizeof (*data));
    size_t n = make_signal (rx, data, code, sf, sps, nper, N, 3e-5, 0.0f, 23u);

    despreader_state_t *c = despreader_create (code, sf, sps, 0.0, 0.0, 0.05,
                                               0.005, 0.0, 0.707, 0.5, N);
    uint8_t            *bits = malloc (nbits);
    size_t              k    = despreader_bits (c, rx, n, bits, nbits);
    CHECK (k >= nbits - 3); /* ~one bit per periods_per_bit periods */
    CHECK (despreader_get_bit_phase (c) == 0); /* boundary at epoch 0 */
    int *dec = malloc (k * sizeof (int));
    for (size_t i = 0; i < k; i++)
      dec[i] = bits[i] ? 1 : -1;
    /* tail: after bit-sync settles, recovered bits match the data */
    CHECK (amb_errors (dec, data, k / 3, k) == 0);
    despreader_destroy (c);
    free (rx);
    free (data);
    free (bits);
    free (dec);
    free (code);
  }

  /* 6. Reset reproducibility */
  {
    const size_t nper = 300;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 21u);
    float complex *rx   = malloc (tsamps * nper * sizeof (*rx));
    int           *data = malloc (nper * sizeof (*data));
    size_t n = make_signal (rx, data, code, sf, sps, nper, 1, 5e-5, 0.0f, 5u);

    despreader_state_t *c   = despreader_create (code, sf, sps, 0.0, 0.0, 0.05,
                                                 0.005, 0.0, 0.707, 0.5, 1);
    float complex      *sym = malloc (nper * sizeof (*sym));
    despreader_steps (c, rx, n, sym, nper);
    double f1 = despreader_get_norm_freq (c),
           l1 = despreader_get_lock_metric (c);
    despreader_reset (c);
    despreader_steps (c, rx, n, sym, nper);
    CHECK (f1 == despreader_get_norm_freq (c));
    CHECK (l1 == despreader_get_lock_metric (c));
    despreader_destroy (c);
    free (rx);
    free (data);
    free (sym);
    free (code);
  }

  /* serializable state — costas + dll children resume; dll borrows this
   * despreader's own code copy after restore.
   * (Moved above the final _fails check: this block used to sit after it,
   * so its own failures could never fail the test.) */
  {
    uint8_t code[31];
    for (int i = 0; i < 31; i++)
      code[i] = (uint8_t)(i & 1);
    float complex rx[256], sym[16];
    for (int i = 0; i < 256; i++)
      rx[i] = (float)(i % 5) - 2.0f + 0.2f * I;
    despreader_state_t *a = despreader_create (code, 31, 2, 0.0, 0.0, 0.05,
                                               0.005, 0.0, 0.707, 0.5, 1);
    despreader_state_t *b = despreader_create (code, 31, 2, 0.0, 0.0, 0.05,
                                               0.005, 0.0, 0.707, 0.5, 1);
    CHECK (a != NULL && b != NULL);
    (void)despreader_steps (a, rx, 256, sym, 16);
    DP_STATE_ROUNDTRIP_TEST (despreader, a, b);
    CHECK (b->epoch_count == a->epoch_count);
    CHECK (b->car.acc == a->car.acc);       /* costas child */
    CHECK (b->code.acc_p == a->code.acc_p); /* dll child */
    CHECK (b->code_copy != NULL && b->code.code == b->code_copy);
    despreader_destroy (a);
    despreader_destroy (b);
  }

  /* telemetry attach — a pure forward to both embedded loops: seven
   * records per code period from steps(), the guarded flush from bits();
   * detach and partial-failure unwind cascade through both children. */
  {
    uint8_t code[31];
    for (int i = 0; i < 31; i++)
      code[i] = (uint8_t)(i & 1);
    enum
    {
      EP  = 62,
      NEP = 20,
      L   = EP * NEP
    };
    float complex rx[L], sym[64];
    dp_tlm_rec_t  recs[512];
    for (int i = 0; i < L; i++)
      rx[i] = (code[(size_t)(i / 2) % 31] & 1u) ? -1.0f : 1.0f;
    dp_tlm_t           *tlm = dp_tlm_create (4096);
    despreader_state_t *ch  = despreader_create (code, 31, 2, 0.0, 0.0, 0.05,
                                                 0.005, 0.0, 0.707, 0.5, 1);
    CHECK (tlm != NULL && ch != NULL);
    CHECK (despreader_set_telemetry (ch, tlm, "ch0", 1) == DP_OK);
    CHECK (dp_tlm_lookup (tlm, "ch0.car.lock") == ch->car.tlm.id_lock);
    CHECK (dp_tlm_lookup (tlm, "ch0.code.e") == ch->code.tlm.id_e);
    CHECK (ch->tlm_ctx == tlm && ch->car.tlm.ctx == tlm
           && ch->code.tlm.ctx == tlm);

    size_t k     = despreader_steps (ch, rx, L, sym, 64);
    size_t n_rec = dp_tlm_read (tlm, recs, 512);
    CHECK (k > 0 && n_rec == 7 * k); /* both loops flush per period */

    /* bits() flushes telemetry too (the guarded in-loop path). */
    uint8_t bit_out[64];
    (void)despreader_bits (ch, rx, L, bit_out, 64);
    CHECK (dp_tlm_read (tlm, recs, 512) > 0);

    /* Detach cascades to both children. */
    CHECK (despreader_set_telemetry (ch, NULL, "ch0", 1) == DP_OK);
    CHECK (ch->tlm_ctx == NULL && ch->car.tlm.ctx == NULL
           && ch->code.tlm.ctx == NULL);
    (void)despreader_steps (ch, rx, L, sym, 64);
    CHECK (dp_tlm_read (tlm, recs, 512) == 0);

    /* Partial registration failure unwinds: leave exactly three free
     * slots — the carrier attach succeeds, the code attach cannot fit,
     * and the whole attach fails with the carrier detached again. */
    char pname[DP_TLM_NAME_MAX];
    for (size_t i = 0;
         dp_tlm_probe_count (tlm) < (size_t)(DP_TLM_MAX_PROBES - 3); i++)
      {
        (void)snprintf (pname, sizeof (pname), "fill%zu", i);
        (void)dp_tlm_probe (tlm, pname, 1);
      }
    CHECK (despreader_set_telemetry (ch, tlm, "nope", 1) == DP_ERR_INVALID);
    CHECK (ch->tlm_ctx == NULL && ch->car.tlm.ctx == NULL
           && ch->code.tlm.ctx == NULL);
    despreader_destroy (ch);
    dp_tlm_destroy (tlm);
  }

  if (_fails)
    {
      fprintf (stderr, "test_despreader_core FAILED (%d)\n", _fails);
      return 1;
    }

  printf ("test_despreader_core PASSED\n");
  return 0;
}
