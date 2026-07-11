/**
 * @file test_costas_core.c
 * @brief Unit tests for the Costas carrier-tracking loop.
 *
 * Tests:
 *   1. Lifecycle / gain math / init==create parity
 *   2. Pull-in — locks a grid of carrier residuals (freq + lock + bits)
 *   3. 180° BPSK ambiguity is tolerated (output correct up to a global flip)
 *   4. Noise robustness — locks under AWGN
 *   5. Dynamic stress — tracks a frequency ramp (Doppler rate) with bounded
 * err
 *   6. Reset reproducibility
 */
#include "costas/costas_core.h"
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

/* Deterministic ±1 BPSK bit stream (xorshift). */
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

/* Box-Muller unit-variance Gaussian (per component). */
static float
gauss (uint32_t *st)
{
  double u1 = (prbs (st) + 2) / 4.0; /* crude but seeded; reseed below */
  (void)u1;
  /* use two PRBS dwords mapped to (0,1) */
  uint32_t a  = (*st ^= *st << 7, *st);
  uint32_t b  = (*st ^= *st >> 9, *st);
  double   r1 = (a + 1.0) / 4294967297.0;
  double   r2 = (b + 1.0) / 4294967297.0;
  return (float)(sqrt (-2.0 * log (r1)) * cos (2.0 * M_PI * r2));
}

/* Build a continuous BPSK-at-symbol-rate signal with carrier residual f0
 * (cycles/sample), optional per-sample frequency ramp (Doppler rate), and
 * optional AWGN sigma per component.  Returns the ±1 bits used in `bits`. */
static void
make_signal (float complex *rx, int *bits, size_t nsym, size_t tsamps,
             double f0, double ramp, float sigma, uint32_t seed)
{
  uint32_t bst = seed, nst = seed ^ 0x9e3779b9u;
  double   phase = 0.0, w = f0 * 2.0 * M_PI;
  size_t   k = 0;
  for (size_t s = 0; s < nsym; s++)
    {
      int b   = prbs (&bst);
      bits[s] = b;
      for (size_t i = 0; i < tsamps; i++, k++)
        {
          float complex c = cexpf ((float)phase * I);
          rx[k]           = (float)b * c;
          if (sigma > 0.0f)
            rx[k] += CMPLXF (sigma * gauss (&nst), sigma * gauss (&nst));
          phase += w;
          w += ramp * 2.0 * M_PI; /* frequency ramps each sample */
        }
    }
}

/* Run the loop over a built signal; report tracked freq, lock, and the
 * ambiguity-tolerant bit-error count over the converged tail. */
static void
run (costas_state_t *c, const float complex *rx, const int *bits, size_t nsym,
     size_t tsamps, double *out_freq, double *out_lock, int *out_biterr)
{
  float complex *sym = malloc (nsym * sizeof (*sym));
  size_t         k   = costas_steps (c, rx, nsym * tsamps, sym, nsym);
  *out_freq          = costas_get_norm_freq (c);
  *out_lock          = costas_get_lock_metric (c);
  /* bit errors over the converged tail (last half), ambiguity-tolerant */
  size_t tail0 = k / 2;
  int    err   = 0;
  for (size_t s = tail0; s < k; s++)
    {
      int dec = (crealf (sym[s]) >= 0.0f) ? 1 : -1;
      if (dec != bits[s])
        err++;
    }
  int n       = (int)(k - tail0);
  *out_biterr = err < n - err ? err : n - err; /* min(err, n-err) */
  free (sym);
}

int
main (void)
{
  int _fails = 0;

  /* ---------------------------------------------------------------- *
   * 1. Lifecycle, gain math, init==create parity                     *
   * ---------------------------------------------------------------- */
  {
    costas_state_t *c = costas_create (0.05, 0.707, 0.01, 16, 0.0);
    CHECK (c != NULL);
    if (!c)
      return 1;
    /* gains derive from the embedded loop_filter (bn,zeta,t=1) */
    CHECK (c->lf.kp > 0.0 && c->lf.ki > 0.0);
    /* seeded NCO frequency == requested residual */
    CHECK (fabs (costas_get_norm_freq (c) - 0.01) < 1e-12);

    costas_state_t v;
    costas_init (&v, 0.05, 0.707, 0.01, 16, 0.0);
    CHECK (v.lf.kp == c->lf.kp && v.lf.ki == c->lf.ki);
    CHECK (v.nco.phase_inc == c->nco.phase_inc);
    costas_destroy (c);
  }

  /* ---------------------------------------------------------------- *
   * 2. Pull-in — a grid of carrier residuals locks                   *
   * ---------------------------------------------------------------- */
  {
    const size_t   tsamps = 16, nsym = 4000;
    float complex *rx    = malloc (nsym * tsamps * sizeof (*rx));
    int           *bits  = malloc (nsym * sizeof (*bits));
    double         f0s[] = { 0.0, 0.001, 0.003, -0.004 };
    for (int t = 0; t < 4; t++)
      {
        make_signal (rx, bits, nsym, tsamps, f0s[t], 0.0, 0.0f, 12345u);
        costas_state_t *c = costas_create (0.05, 0.707, 0.0, tsamps, 0.0);
        double          f, lk;
        int             be;
        run (c, rx, bits, nsym, tsamps, &f, &lk, &be);
        CHECK (fabs (f - f0s[t]) < 2e-4); /* tracked the residual    */
        CHECK (lk > 0.9);                 /* phase-locked            */
        CHECK (be == 0);                  /* zero bit errors on tail */
        costas_destroy (c);
      }
    free (rx);
    free (bits);
  }

  /* ---------------------------------------------------------------- *
   * 3. 180° BPSK ambiguity — output is correct up to a global flip   *
   * (run() already scores ambiguity-tolerant; assert a flipped seed) *
   * ---------------------------------------------------------------- */
  {
    const size_t   tsamps = 16, nsym = 3000;
    float complex *rx   = malloc (nsym * tsamps * sizeof (*rx));
    int           *bits = malloc (nsym * sizeof (*bits));
    make_signal (rx, bits, nsym, tsamps, 0.002, 0.0, 0.0f, 777u);
    costas_state_t *c = costas_create (0.05, 0.707, 0.0, tsamps, 0.0);
    double          f, lk;
    int             be;
    run (c, rx, bits, nsym, tsamps, &f, &lk, &be);
    CHECK (be == 0); /* min(err, n-err)==0 even if globally inverted  */
    CHECK (lk > 0.9);
    costas_destroy (c);
    free (rx);
    free (bits);
  }

  /* ---------------------------------------------------------------- *
   * 4. Noise robustness — locks under AWGN (per-symbol SNR is high   *
   * after tsamps-fold coherent integration)                          *
   * ---------------------------------------------------------------- */
  {
    const size_t   tsamps = 16, nsym = 5000;
    float complex *rx   = malloc (nsym * tsamps * sizeof (*rx));
    int           *bits = malloc (nsym * sizeof (*bits));
    /* sigma=1.0 per component → ~ -3 dB per-sample SNR; +12 dB from the
     * 16-fold I&D → comfortably locked. */
    make_signal (rx, bits, nsym, tsamps, 0.0015, 0.0, 1.0f, 2024u);
    costas_state_t *c = costas_create (0.03, 0.707, 0.0, tsamps, 0.0);
    double          f, lk;
    int             be;
    run (c, rx, bits, nsym, tsamps, &f, &lk, &be);
    CHECK (fabs (f - 0.0015) < 5e-4);
    CHECK (lk > 0.7);
    CHECK (be == 0);
    costas_destroy (c);
    free (rx);
    free (bits);
  }

  /* ---------------------------------------------------------------- *
   * 5. Dynamic stress — track a frequency ramp (Doppler rate)        *
   * ---------------------------------------------------------------- */
  {
    const size_t   tsamps = 16, nsym = 6000;
    float complex *rx   = malloc (nsym * tsamps * sizeof (*rx));
    int           *bits = malloc (nsym * sizeof (*bits));
    /* start at 0, ramp the carrier to a final offset; 2nd-order loop
     * tracks a constant rate with bounded (small) steady error. */
    double ramp = 5e-9; /* cycles/sample per sample */
    make_signal (rx, bits, nsym, tsamps, 0.0, ramp, 0.0f, 99u);
    double          final_f0 = ramp * (double)(nsym * tsamps);
    costas_state_t *c        = costas_create (0.06, 0.707, 0.0, tsamps, 0.0);
    double          f, lk;
    int             be;
    run (c, rx, bits, nsym, tsamps, &f, &lk, &be);
    CHECK (fabs (f - final_f0) < 1e-3); /* follows the moving carrier */
    CHECK (lk > 0.85);
    CHECK (be == 0);
    costas_destroy (c);
    free (rx);
    free (bits);
  }

  /* ---------------------------------------------------------------- *
   * 6. Reset reproducibility — run #2 == run #1 after reset          *
   * ---------------------------------------------------------------- */
  {
    const size_t   tsamps = 16, nsym = 1500;
    float complex *rx   = malloc (nsym * tsamps * sizeof (*rx));
    int           *bits = malloc (nsym * sizeof (*bits));
    make_signal (rx, bits, nsym, tsamps, 0.002, 0.0, 0.0f, 55u);
    costas_state_t *c = costas_create (0.05, 0.707, 0.0, tsamps, 0.0);
    double          f1, lk1;
    int             be1;
    run (c, rx, bits, nsym, tsamps, &f1, &lk1, &be1);
    costas_reset (c);
    double f2, lk2;
    int    be2;
    run (c, rx, bits, nsym, tsamps, &f2, &lk2, &be2);
    CHECK (f1 == f2 && lk1 == lk2 && be1 == be2);
    costas_destroy (c);
    free (rx);
    free (bits);
  }

  /* ---------------------------------------------------------------- *
   * 7. FLL assist widens pull-in — a residual too large for the bare *
   * PLL is acquired once the FLL assist is enabled.                  *
   * ---------------------------------------------------------------- */
  {
    const size_t   tsamps = 16, nsym = 6000;
    float complex *rx   = malloc (nsym * tsamps * sizeof (*rx));
    int           *bits = malloc (nsym * sizeof (*bits));
    /* ~0.8 rad/symbol residual — beyond the narrow PLL's pull-in. */
    double f0 = 0.008;
    make_signal (rx, bits, nsym, tsamps, f0, 0.0, 0.0f, 31u);
    double f, lk;
    int    be;

    /* Pure PLL (bn_fll = 0): fails to lock onto the large residual. */
    costas_state_t *pll = costas_create (0.01, 0.707, 0.0, tsamps, 0.0);
    run (pll, rx, bits, nsym, tsamps, &f, &lk, &be);
    int pll_locked = (fabs (f - f0) < 5e-4) && (lk > 0.9);
    CHECK (!pll_locked); /* the bare PLL does NOT acquire it */
    costas_destroy (pll);

    /* FLL-assisted (bn_fll > 0): the wide frequency discriminator pulls
     * the integrator on, and the loop locks. */
    costas_state_t *fll = costas_create (0.01, 0.707, 0.0, tsamps, 0.03);
    run (fll, rx, bits, nsym, tsamps, &f, &lk, &be);
    CHECK (fabs (f - f0) < 5e-4); /* tracked the large residual */
    CHECK (lk > 0.9);             /* locked */
    CHECK (be == 0);              /* zero bit errors on the tail */
    costas_destroy (fll);
    free (rx);
    free (bits);
  }

  /* serializable state — whole-struct snapshot resumes the loop bit-for-bit.
   */
  {
    enum
    {
      L   = 600,
      CUT = 251,
      CAP = 600
    };
    float _Complex *rx   = malloc (L * sizeof (float _Complex));
    float _Complex *outA = malloc (CAP * sizeof (float _Complex));
    float _Complex *outB = malloc (CAP * sizeof (float _Complex));
    for (size_t i = 0; i < L; i++)
      rx[i] = cosf (0.02f * (float)i) + I * sinf (0.02f * (float)i);

    costas_state_t *a  = costas_create (0.01, 0.707, 0.0, 4, 0.0);
    size_t          nA = costas_steps (a, rx, L, outA, CAP);
    costas_destroy (a);

    costas_state_t *r1   = costas_create (0.01, 0.707, 0.0, 4, 0.0);
    size_t          nB   = costas_steps (r1, rx, CUT, outB, CAP);
    size_t          sb   = costas_state_bytes (r1);
    void           *blob = malloc (sb);
    costas_get_state (r1, blob);
    costas_destroy (r1);

    costas_state_t *r2 = costas_create (0.01, 0.707, 0.0, 4, 0.0);
    CHECK (costas_set_state (r2, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF;
    CHECK (costas_set_state (r2, blob) == DP_ERR_INVALID);
    ((char *)blob)[0] ^= (char)0xFF;
    nB += costas_steps (r2, rx + CUT, L - CUT, outB + nB, CAP - nB);
    costas_destroy (r2);
    free (blob);

    CHECK (nA == nB);
    for (size_t i = 0; i < nA && i < nB; i++)
      CHECK (crealf (outA[i]) == crealf (outB[i])
             && cimagf (outA[i]) == cimagf (outB[i]));
    free (rx);
    free (outA);
    free (outB);
  }

  /* verify-counted carrier lock decision: a phase-locked BPSK stream
   * declares after the default 8-symbol verify run; noise-only input
   * never does; reset drops the lock and keeps the configured rule. */
  {
    enum
    {
      TS = 16,
      NS = 256
    };
    float complex rx[TS * NS], out[NS];
    /* constant BPSK at a locked phase: metric EMA -> 1 quickly */
    for (int i = 0; i < TS * NS; i++)
      rx[i] = ((i / (TS * 4)) % 2 ? -1.0f : 1.0f) + 0.0f * I;
    costas_state_t *c = costas_create (0.05, 0.707, 0.0, TS, 0.0);
    CHECK (c != NULL);
    CHECK (costas_get_locked (c) == 0); /* fresh: unlocked */
    CHECK (c->lock.up_thresh == 0.85 && c->lock.down_thresh == 0.78);
    CHECK (c->lock.n_up == 8 && c->lock.n_down == 32);
    (void)costas_steps (c, rx, TS * NS, out, NS);
    CHECK (costas_get_locked (c) == 1);
    CHECK (costas_get_lock_metric (c) > 0.85);

    /* reset drops the decision but keeps the rule */
    costas_reset (c);
    CHECK (costas_get_locked (c) == 0);
    CHECK (c->lock.n_down == 32);

    /* configure_lock re-tunes; an unreachable declare threshold never
     * locks even on the clean stream */
    costas_configure_lock (c, 2.0, 1.9, 8, 32);
    (void)costas_steps (c, rx, TS * NS, out, NS);
    CHECK (costas_get_locked (c) == 0);
    costas_destroy (c);

    /* noise only: |cos(theta)| EMA hovers near 2/pi ~ 0.64, well under
     * the 0.85 declare threshold -> never declares */
    uint32_t        st = 77u;
    costas_state_t *n  = costas_create (0.05, 0.707, 0.0, TS, 0.0);
    CHECK (n != NULL);
    for (int i = 0; i < TS * NS; i++)
      {
        st ^= st << 13;
        st ^= st >> 17;
        st ^= st << 5;
        double u1 = ((double)st + 1.0) / 4294967297.0;
        st ^= st << 13;
        st ^= st >> 17;
        st ^= st << 5;
        double u2 = ((double)st + 1.0) / 4294967297.0;
        double m  = sqrt (-2.0 * log (u1));
        rx[i]     = (float complex) (m * cos (2.0 * M_PI * u2)
                                     + m * sin (2.0 * M_PI * u2) * I);
      }
    (void)costas_steps (n, rx, TS * NS, out, NS);
    CHECK (costas_get_locked (n) == 0);
    CHECK (costas_get_lock_metric (n) < 0.85);
    costas_destroy (n);
  }

  /* telemetry attach — four records per dumped symbol; blobs stay
   * attachment-independent and a live attachment survives set_state. */
  {
    enum
    {
      TS = 16,
      NS = 64,
      L  = TS * NS
    };
    float complex rx[L], out[NS];
    dp_tlm_rec_t  recs[512];
    for (int i = 0; i < L; i++)
      rx[i] = ((i / (TS * 4)) % 2 ? -1.0f : 1.0f) + 0.0f * I;
    dp_tlm_t       *tlm = dp_tlm_create (4096);
    costas_state_t *c   = costas_create (0.05, 0.707, 0.0, TS, 0.0);
    CHECK (tlm != NULL && c != NULL);
    CHECK (costas_set_telemetry (c, tlm, "car", 1) == DP_OK);
    CHECK (dp_tlm_lookup (tlm, "car.lock") == c->tlm.id_lock);
    CHECK (dp_tlm_lookup (tlm, "car.e") == c->tlm.id_e);
    CHECK (dp_tlm_lookup (tlm, "car.freq") == c->tlm.id_freq);
    CHECK (dp_tlm_lookup (tlm, "car.locked") == c->tlm.id_locked);

    size_t k = costas_steps (c, rx, L, out, NS);
    CHECK (k == NS);
    size_t n_rec = dp_tlm_read (tlm, recs, 512);
    CHECK (n_rec == 4 * NS); /* lock + e + freq + locked per symbol */
    /* The last records mirror the tracked state (flush order:
     * lock, e, freq, locked). */
    CHECK (recs[n_rec - 2].value == (float)c->nco.norm_freq);
    CHECK (recs[n_rec - 1].value == (float)costas_get_locked (c));

    /* Blobs zero the attachment (deterministic) and set_state into an
     * attached instance preserves that instance's live attachment. */
    size_t sb = costas_state_bytes (c);
    void  *b1 = malloc (sb), *b2 = malloc (sb);
    costas_get_state (c, b1);
    costas_state_t *d = costas_create (0.05, 0.707, 0.0, TS, 0.0);
    CHECK (d != NULL);
    CHECK (costas_set_telemetry (d, tlm, "car2", 2) == DP_OK);
    CHECK (costas_set_state (d, b1) == DP_OK);
    CHECK (d->tlm.ctx == tlm);
    CHECK (d->tlm.id_e == dp_tlm_lookup (tlm, "car2.e"));
    costas_get_state (d, b2);
    CHECK (memcmp (b1, b2, sb) == 0); /* attachment-independent bytes */
    free (b1);
    free (b2);
    costas_destroy (d);

    /* Detach: probe sites revert to the single-branch cost. */
    CHECK (costas_set_telemetry (c, NULL, "car", 1) == DP_OK);
    CHECK (c->tlm.ctx == NULL);
    (void)costas_steps (c, rx, L, out, NS);
    CHECK (dp_tlm_read (tlm, recs, 512) == 0);

    /* A full probe table fails the attach whole. */
    char pname[DP_TLM_NAME_MAX];
    for (size_t i = 0; dp_tlm_probe_count (tlm) < DP_TLM_MAX_PROBES; i++)
      {
        (void)snprintf (pname, sizeof (pname), "fill%zu", i);
        (void)dp_tlm_probe (tlm, pname, 1);
      }
    CHECK (costas_set_telemetry (c, tlm, "nope", 1) == DP_ERR_INVALID);
    CHECK (c->tlm.ctx == NULL);
    costas_destroy (c);
    dp_tlm_destroy (tlm);
  }

  if (_fails)
    {
      fprintf (stderr, "test_costas_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_costas_core PASSED\n");
  return 0;
}
