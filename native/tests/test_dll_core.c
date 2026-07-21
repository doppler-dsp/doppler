/**
 * @file test_dll_core.c
 * @brief Unit tests for the DLL (early/prompt/late code-tracking loop).
 *
 * Tests:
 *   1. Lifecycle / NULL-code guard / init==create parity
 *   1b. dll_init defends against a dirty caller stack (rate_aid init)
 *   2. On-time alignment — discriminator ~0, code_rate ~1
 *   3. Code Doppler — code_rate converges to the incoming chip rate
 *   4. Static phase offset is pulled in (discriminator decays)
 *   5. Reset reproducibility
 *   6. segments > 1: sub-epoch partials recover an async symbol clock
 *   7. Always-on lock detector: locks on signal, not on noise
 *   8. Long-run false-lock regression (segments=1, ~4188 periods, several
 *      noise seeds, zero Doppler/carrier)
 */
#include "dll/dll_core.h"
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

/* xorshift ±1 BPSK data bit (one per code period). */
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

/* Build a deterministic 0/1 spreading code (xorshift bits). */
static void
make_code (uint8_t *code, size_t sf, uint32_t seed)
{
  uint32_t st = seed;
  for (size_t i = 0; i < sf; i++)
    code[i] = prbs (&st) > 0 ? 0u : 1u;
}

/* Carrier-free spread signal at code rate (1+delta), BPSK data per period
 * (random, or held at +1 when `const_data` to isolate code tracking from the
 * data-symbol vs code-period async). `sps` samples per nominal chip. Returns
 * the sample count. */
static size_t
make_signal (float complex *rx, const uint8_t *code, size_t sf, size_t sps,
             double delta, size_t nper, uint32_t seed, int const_data)
{
  uint32_t dst    = seed;
  size_t   tsamps = sf * sps;
  double   inv    = 1.0 / (double)sps;
  int      data   = prbs (&dst);
  size_t   k      = 0;
  double   cph    = 0.0; /* incoming code phase, chips */
  for (size_t p = 0; p < nper; p++)
    {
      data = const_data ? 1 : prbs (&dst);
      for (size_t i = 0; i < tsamps; i++, k++)
        {
          size_t idx  = (size_t)fmod (cph, (double)sf);
          float  csgn = (code[idx] & 1u) ? -1.0f : 1.0f;
          rx[k]       = (float)data * csgn;
          cph += inv * (1.0 + delta); /* incoming chip rate */
        }
    }
  return k;
}

/* Unit-variance complex Gaussian (Box-Muller from xorshift); 0.5 variance per
 * component so E|z|^2 = 1 — a noise-only stream for the lock detector. */
static float complex
cgauss (uint32_t *st)
{
  *st ^= *st << 13;
  *st ^= *st >> 17;
  *st ^= *st << 5;
  uint32_t a = *st;
  *st ^= *st << 13;
  *st ^= *st >> 17;
  *st ^= *st << 5;
  uint32_t b   = *st;
  double   u1  = ((double)a + 1.0) / 4294967297.0;
  double   u2  = ((double)b + 1.0) / 4294967297.0;
  double   mag = sqrt (-log (u1)); /* sqrt(-2 ln u1)/sqrt(2) */
  double   th  = 6.283185307179586 * u2;
  return (float)(mag * cos (th)) + (float)(mag * sin (th)) * I;
}

int
main (void)
{
  int _fails = 0;

  /* ---------------------------------------------------------------- *
   * 1. Lifecycle, NULL-code guard, init==create parity               *
   * ---------------------------------------------------------------- */
  {
    CHECK (dll_create (NULL, 0, 2, 0.0, 0.01, 0.707, 0.5, 1) == NULL);

    uint8_t code[31];
    make_code (code, 31, 1u);
    dll_state_t *c = dll_create (code, 31, 2, 0.0, 0.02, 0.707, 0.5, 1);
    CHECK (c != NULL);
    if (!c)
      return 1;
    CHECK (c->lf.kp > 0.0 && c->lf.ki > 0.0);
    CHECK (c->sf == 31 && c->sps == 2);
    CHECK (dll_get_code_rate (c) == 1.0);

    dll_state_t v;
    dll_init (&v, code, 31, 2, 0.0, 0.02, 0.707, 0.5);
    CHECK (v.lf.kp == c->lf.kp && v.lf.ki == c->lf.ki);
    CHECK (v.owns_code == 0);  /* init borrows */
    CHECK (c->owns_code == 1); /* create copies */
    dll_destroy (c);
  }

  /* ---------------------------------------------------------------- *
   * 1b. dll_init defends against a dirty caller stack                 *
   *                                                                  *
   * dll_init() does an in-place init of a caller-owned (often stack) *
   * dll_state_t, so every field it does not explicitly set starts as *
   * whatever garbage the caller's memory held. rate_aid (carrier-    *
   * aiding config, set only by dll_set_rate_aid) was the one field   *
   * the zero-against-garbage block missed: on a clean stack it read  *
   * 0 (Linux), but a NaN there (0xFF-filled stack, seen on macOS)    *
   * made phase_inc = nco_norm_to_inc(inv_tsamps*(1+NaN)+ctrl) cast   *
   * to 0, freezing the code NCO permanently (validate_dll_jitter     *
   * #82). Poison the whole struct, init, and assert the loop still   *
   * steers a live, wrapping NCO.                                     *
   * ---------------------------------------------------------------- */
  {
    uint8_t code[31];
    make_code (code, 31, 1u);
    dll_state_t g;
    memset (&g, 0xFF, sizeof g); /* 0xFF doubles are NaN — the macOS case */
    dll_init (&g, code, 31, 2, 0.0, 0.002, 0.707, 0.5);
    CHECK (g.rate_aid == 0.0);         /* zeroed, not NaN */
    CHECK (g.code_nco.phase_inc > 0u); /* NCO not frozen */
    uint32_t inc0 = g.code_nco.phase_inc;
    /* run a clean aligned epoch; the NCO must keep a sane (nonzero, ~nominal)
       phase_inc rather than collapsing to 0. */
    float complex sig[31 * 2];
    for (size_t ci = 0; ci < 31; ci++)
      {
        float vv    = (code[ci] & 1u) ? -1.0f : 1.0f;
        sig[ci * 2] = sig[ci * 2 + 1] = vv;
      }
    int wraps = 0;
    for (int ep = 0; ep < 4; ep++)
      for (size_t i = 0; i < 31 * 2; i++)
        if (dll_accumulate (&g, sig[i]))
          {
            dll_update (&g);
            g.acc_e = g.acc_p = g.acc_l = 0.0f;
            wraps++;
          }
    CHECK (wraps >= 3);                       /* keeps wrapping ~once/epoch */
    CHECK (g.code_nco.phase_inc > inc0 / 2u); /* still near nominal, not 0 */
  }

  /* ---------------------------------------------------------------- *
   * 2. On-time alignment — E ~ L, discriminator ~0, code_rate ~1     *
   * ---------------------------------------------------------------- */
  {
    const size_t sf = 63, sps = 4, nper = 400;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 7u);
    float complex *rx = malloc (sf * sps * nper * sizeof (*rx));
    size_t         n  = make_signal (rx, code, sf, sps, 0.0, nper, 3u, 0);

    dll_state_t   *d   = dll_create (code, sf, sps, 0.0, 0.02, 0.707, 0.5, 1);
    float complex *sym = malloc (nper * sizeof (*sym));
    size_t         k   = dll_steps (d, rx, n, sym, nper);
    CHECK (k >= nper - 2 && k <= nper);           /* ~one prompt per period */
    CHECK (fabs (dll_get_last_error (d)) < 0.05); /* E ~ L on-time */
    CHECK (fabs (dll_get_code_rate (d) - 1.0) < 1e-3);
    dll_destroy (d);
    free (rx);
    free (sym);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 3. Code Doppler — code_rate converges to the incoming chip rate  *
   * ---------------------------------------------------------------- */
  {
    const size_t sf = 63, sps = 4, nper = 1500;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 11u);
    float complex *rx    = malloc (sf * sps * nper * sizeof (*rx));
    double         delta = 5e-4; /* incoming code runs 0.05% fast */
    size_t         n     = make_signal (rx, code, sf, sps, delta, nper, 9u, 1);

    /* half-chip E/L discriminator is steep — keep the loop BW low. */
    dll_state_t   *d   = dll_create (code, sf, sps, 0.0, 0.005, 0.707, 0.5, 1);
    float complex *sym = malloc (nper * sizeof (*sym));
    size_t         k   = dll_steps (d, rx, n, sym, nper);
    /* the loop must speed its replica up to match the incoming rate */
    CHECK (fabs (dll_get_code_rate (d) - (1.0 + delta)) < 1e-4);
    /* sub-chip lock holds: the prompt despreads cleanly over the run tail
       (mean |Re prompt| well above 0; the code-phase tracking follows the
       sliding code phase without the integer-sample staircase). Threshold
       0.85, not the pre-redesign 0.9: the 2-samples/chip interpolated
       replica (docs/design/async-despreader-working-design.md) is a
       simpler point-sample model than the old dwell-width-aware exact
       matched-filter integral it replaced, and costs a small, expected
       amount of despread gain (observed ~0.87 here; ~0.88 in the Python
       prototype that validated this redesign) -- not a regression. */
    size_t lo = k / 2;
    double pm = 0.0;
    for (size_t j = lo; j < k; j++)
      pm += fabs (crealf (sym[j]));
    CHECK (k > lo && pm / (double)(k - lo) > 0.85);
    dll_destroy (d);
    free (rx);
    free (sym);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 4. Static phase offset is pulled in (discriminator decays)       *
   * ---------------------------------------------------------------- */
  {
    const size_t sf = 63, sps = 4, nper = 800;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 13u);
    float complex *rx = malloc (sf * sps * nper * sizeof (*rx));
    size_t         n  = make_signal (rx, code, sf, sps, 0.0, nper, 17u, 0);

    /* seed the replica 0.4 chips off — the loop must realign it */
    dll_state_t   *d   = dll_create (code, sf, sps, 0.4, 0.005, 0.707, 0.5, 1);
    float complex *sym = malloc (nper * sizeof (*sym));
    /* early discriminator (first few periods) should be non-trivial */
    dll_steps (d, rx, sf * sps * 3, sym, 3);
    double early_err = fabs (dll_get_last_error (d));
    dll_steps (d, rx + sf * sps * 3, n - sf * sps * 3, sym, nper);
    double late_err = fabs (dll_get_last_error (d));
    CHECK (early_err > 0.05); /* started misaligned */
    CHECK (late_err < 0.05);  /* pulled in */
    dll_destroy (d);
    free (rx);
    free (sym);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 5. Reset reproducibility                                         *
   * ---------------------------------------------------------------- */
  {
    const size_t sf = 31, sps = 2, nper = 300;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 21u);
    float complex *rx = malloc (sf * sps * nper * sizeof (*rx));
    size_t         n  = make_signal (rx, code, sf, sps, 3e-4, nper, 5u, 0);

    dll_state_t   *d   = dll_create (code, sf, sps, 0.0, 0.02, 0.707, 0.5, 1);
    float complex *sym = malloc (nper * sizeof (*sym));
    dll_steps (d, rx, n, sym, nper);
    double r1 = dll_get_code_rate (d), e1 = dll_get_last_error (d);
    dll_reset (d);
    dll_steps (d, rx, n, sym, nper);
    double r2 = dll_get_code_rate (d), e2 = dll_get_last_error (d);
    CHECK (r1 == r2 && e1 == e2);
    dll_destroy (d);
    free (rx);
    free (sym);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 6. segments > 1: sub-epoch partials recover an async symbol clock *
   * ---------------------------------------------------------------- */
  {
    const size_t sf = 63, sps = 4, K = 4, nsym = 2000;
    size_t       te   = sf * sps;
    double       dsym = 3e-3, tsym = (double)te * (1.0 + dsym);
    double       phi  = 0.37 * (double)te;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 11u);
    size_t         N    = (size_t)(nsym * tsym) + 2 * te;
    float complex *rx   = malloc (N * sizeof (*rx));
    float complex *out  = malloc (N * sizeof (*out));
    int           *data = malloc ((nsym + 8) * sizeof (int));
    uint32_t       ds   = 7u;
    for (size_t i = 0; i < nsym + 6; i++)
      {
        ds ^= ds << 13;
        ds ^= ds >> 17;
        ds ^= ds << 5;
        data[i] = (ds & 1u) ? 1 : -1;
      }
    for (size_t nn = 0; nn < N; nn++)
      {
        long s = (long)floor (((double)nn - phi) / tsym);
        if (s < 0)
          s = 0;
        if (s >= (long)nsym + 6)
          s = nsym + 5;
        size_t ci = (nn / sps) % sf; /* code-aligned (no code Doppler) */
        float  cs = (code[ci] & 1u) ? -1.0f : 1.0f;
        rx[nn]    = (float)data[s] * cs;
      }
    dll_state_t *d   = dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, K);
    size_t       np  = dll_steps (d, rx, N, out, N);
    size_t       nep = N / te;
    CHECK (dll_get_segments (d) == K);
    CHECK (np >= (nep - 1) * K && np <= (nep + 1) * K);
    /* genie symbol despread on the partials (known timing) recovers the data
     */
    double *acc = calloc (nsym + 8, sizeof (double));
    for (size_t pp = 0; pp < np; pp++)
      {
        double t = (double)te * ((double)pp + 0.5) / (double)K;
        long   s = (long)floor ((t - phi) / tsym);
        if (s >= 0 && s < (long)nsym)
          acc[s] += creal (out[pp]);
      }
    long err = 0;
    for (size_t s = 2; s < nsym - 2; s++)
      if ((acc[s] >= 0 ? 1 : -1) != data[s])
        err++;
    CHECK (err == 0); /* partials recover the asynchronous data */
    /* No code Doppler here (ci is code-aligned above): a well-behaved loop
       should settle near code_rate=1 with a small last_error, not pinned at
       DLL_DISC_CLAMP every epoch (a real bug -- the segments>1 discriminator
       once mixed a tsamps-normalised pp against raw-scale ep/lp, off by
       roughly tsamps^2, so it saturated on essentially every epoch). */
    CHECK (fabs (dll_get_code_rate (d) - 1.0) < 1e-3);
    CHECK (fabs (dll_get_last_error (d)) < 0.5);
    dll_destroy (d);
    free (acc);
    free (rx);
    free (out);
    free (data);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 7. Always-on lock detector: locks on signal, not on noise        *
   * ---------------------------------------------------------------- */
  {
    const size_t sf = 63, sps = 4, K = 4, nper = 3000;
    size_t       te   = sf * sps;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 11u);
    float complex *rx  = malloc (te * nper * sizeof (*rx));
    float complex *out = malloc (te * nper * sizeof (*out));

    /* signal present (const data, code-aligned): strong despread -> lock.
       The default config (pfa=1e-3, 20 looks, threshold ~8.567) is applied at
       create, so the detector works with no configure_lock call. */
    size_t       n = make_signal (rx, code, sf, sps, 0.0, nper, 9u, 1);
    dll_state_t *d = dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, K);
    CHECK (dll_get_locked (d) == 0); /* fresh: unlocked */
    CHECK (dll_get_lock_stat (d) == 0.0);
    dll_steps (d, rx, n, out, te * nper);
    CHECK (dll_get_locked (d) == 1);
    CHECK (dll_get_lock_stat (d) > 8.567); /* default CFAR threshold */
    dll_destroy (d);

    /* noise only: prompt power matches the off-peak reference, so the
       statistic sits near sqrt(2*N) ~ 6.3 and stays below threshold. */
    uint32_t st = 4242u;
    for (size_t i = 0; i < te * nper; i++)
      rx[i] = cgauss (&st);
    dll_state_t *dn = dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, K);
    dll_steps (dn, rx, te * nper, out, te * nper);
    CHECK (dll_get_locked (dn) == 0);
    CHECK (dll_get_lock_stat (dn) < 8.567);
    CHECK (dll_get_lock_stat (dn) > 3.0); /* near sqrt(40), not degenerate */
    CHECK (dll_get_noise_est (dn) > 0.0);
    /* configure_lock retunes the threshold; an unreachable one never locks. */
    dll_configure_lock_raw (dn, 1e9, 1e9, 20, 1.0 / 1024.0, 1, 1);
    CHECK (dll_get_lock_stat (dn) == 0.0); /* retune clears the statistic */
    dll_steps (dn, rx, te * nper, out, te * nper);
    CHECK (dll_get_locked (dn) == 0);
    dll_destroy (dn);

    /* Verify-counted declare: the default config (pfa=1e-3 -> n_up=2)
       needs two CONSECUTIVE above-threshold decisions. K=4 partials per
       period x 5 periods = 20 looks = exactly one decision window, so one
       window of strong signal leaves the loop unlocked with the verify
       run in flight; the second window declares. Both windows are drawn
       from ONE continuous 11-period signal (not the same 5 periods
       replayed) -- replaying the identical buffer for the second call
       would re-inject it right where the first call's phase left off,
       an artificial discontinuity at the seam that isn't a real data
       transition and measurably (not just cosmetically) degrades one
       epoch's power. A small margin beyond exactly 5 periods' worth of
       samples (half a partial-segment width, well under a quarter
       period) gives each window's last epoch room to complete its wrap
       even though the fixed-point loop's phase_inc drifts by a few PPM
       during early convergence -- exactly 5*tsamps samples is otherwise
       a zero-slack edge case, not a meaningful assertion. */
    size_t margin = sf * sps / 8;
    size_t nv     = sf * sps * 5 + margin;
    make_signal (rx, code, sf, sps, 0.0, 11, 9u, 1);
    dll_state_t *dv = dll_create (code, sf, sps, 0.0, 0.002, 0.707, 0.5, K);
    dll_steps (dv, rx, nv, out, nv);
    CHECK (dv->lock.cnt == 1);
    CHECK (dll_get_locked (dv) == 0);
    dll_steps (dv, rx + nv, nv, out, nv);
    CHECK (dll_get_locked (dv) == 1);
    dll_destroy (dv);
    free (rx);
    free (out);
    free (code);
  }

  /* serializable state — loop_filter child + correlators resume; the borrowed
   * code pointer is this instance's, preserved across set_state.
   * (Moved above the final _fails check: this block used to sit after it,
   * so its own failures could never fail the test.) */
  {
    uint8_t code[31];
    for (int i = 0; i < 31; i++)
      code[i] = (uint8_t)(i & 1);
    dll_state_t *a = dll_create (code, 31, 2, 0.0, 0.02, 0.707, 0.5, 1);
    dll_state_t *b = dll_create (code, 31, 2, 0.0, 0.02, 0.707, 0.5, 1);
    CHECK (a != NULL && b != NULL);
    for (int i = 0; i < 80; i++)
      dll_accumulate (a, (float)(i % 7) - 3.0f + 0.5f * I);
    DP_STATE_ROUNDTRIP_TEST (dll, a, b);
    CHECK (b->chip_pos == a->chip_pos && b->acc_p == a->acc_p);
    CHECK (b->lf.integ == a->lf.integ);            /* child resumed */
    CHECK (b->code != NULL && b->code != a->code); /* code preserved */
    dll_destroy (a);
    dll_destroy (b);
  }

  /* lock config — pfa face is C-first now: the create-time default is the
   * precise detection-module threshold (no baked constant), the EMA alpha
   * comes from the det_ema_alpha estimator-SNR contract, and bad pfa is
   * rejected without touching the live config. */
  {
    uint8_t code[31];
    for (int i = 0; i < 31; i++)
      code[i] = (uint8_t)(i & 1);
    dll_state_t *d = dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 1);
    CHECK (d != NULL);
    /* create-time default == the exact caller-path config; both lockdet
     * thresholds carry the CFAR eta (no level hysteresis by default) and
     * the declare verify count derives from the pfa:
     * det_verify_count(1e-3, 1e-6) = 2. */
    CHECK (d->lock.up_thresh == det_threshold_noncoherent (1e-3, 20));
    CHECK (d->lock.down_thresh == d->lock.up_thresh);
    CHECK (d->lock.n_up == 2 && d->lock.n_down == 2);
    CHECK (fabs (d->lock_alpha - 1.0 / 1024.0) < 1e-15); /* auto floor */

    /* auto derivation follows 1/alpha = max(32*N, 1024) */
    CHECK (dll_configure_lock (d, 1e-3, 64, 0.0) == DP_OK);
    CHECK (fabs (d->lock_alpha - 1.0 / 2048.0) < 1e-15);
    CHECK (d->n_looks == 64);

    /* explicit reference SNR overrides the auto sizing */
    CHECK (dll_configure_lock (d, 1e-2, 20, 20.0) == DP_OK);
    CHECK (fabs (d->lock_alpha - 2.0 / 101.0) < 1e-15);
    CHECK (d->lock.up_thresh == det_threshold_noncoherent (1e-2, 20));

    /* bad pfa: rejected whole, live config untouched */
    double thr = d->lock.up_thresh, alp = d->lock_alpha;
    CHECK (dll_configure_lock (d, 0.0, 20, 0.0) == DP_ERR_INVALID);
    CHECK (dll_configure_lock (d, 1.0, 20, 0.0) == DP_ERR_INVALID);
    CHECK (dll_configure_lock (d, -1.0, 20, 0.0) == DP_ERR_INVALID);
    CHECK (d->lock.up_thresh == thr && d->lock_alpha == alp);

    /* n_looks = 0 clamps to 1 (auto floor still applies) */
    CHECK (dll_configure_lock (d, 1e-3, 0, 0.0) == DP_OK);
    CHECK (d->n_looks == 1);
    CHECK (fabs (d->lock_alpha - 1.0 / 1024.0) < 1e-15);
    dll_destroy (d);
  }

  /* telemetry attach — four records per code epoch in both the coherent
   * (segments == 1) and partial-correlation (segments > 1) loops; blobs
   * stay attachment-independent; a live attachment survives set_state. */
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
    float complex rx[L], out[256];
    dp_tlm_rec_t  recs[512];
    for (int i = 0; i < L; i++)
      rx[i] = (code[(size_t)(i / 2) % 31] & 1u) ? -1.0f : 1.0f;
    dp_tlm_t    *tlm = dp_tlm_create (4096);
    dll_state_t *d   = dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 1);
    CHECK (tlm != NULL && d != NULL);
    CHECK (dll_set_telemetry (d, tlm, "code", 1) == DP_OK);
    CHECK (dp_tlm_lookup (tlm, "code.e") == d->tlm.id_e);
    CHECK (dp_tlm_lookup (tlm, "code.rate") == d->tlm.id_rate);
    CHECK (dp_tlm_lookup (tlm, "code.lock") == d->tlm.id_lock);
    CHECK (dp_tlm_lookup (tlm, "code.locked") == d->tlm.id_locked);

    size_t k     = dll_steps (d, rx, L, out, 256);
    size_t n_rec = dp_tlm_read (tlm, recs, 512);
    CHECK (k > 0 && n_rec == 4 * k); /* e + rate + lock + locked / epoch */
    /* The final epoch's rate/lock/locked records mirror the tracked state
     * (flush order per epoch: e, rate, lock, locked). */
    CHECK (recs[n_rec - 3].value == (float)d->code_rate);
    CHECK (recs[n_rec - 2].value == (float)d->lock_stat);
    CHECK (recs[n_rec - 1].value == (float)dll_get_locked (d));

    /* segments > 1: the partial loop flushes once per epoch (an epoch is
     * `segments` emitted partials), through the same literal-tlm split. */
    dll_state_t *s2 = dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 2);
    CHECK (s2 != NULL);
    CHECK (dll_set_telemetry (s2, tlm, "code2", 1) == DP_OK);
    size_t k2 = dll_steps (s2, rx, L, out, 256);
    size_t n2 = dp_tlm_read (tlm, recs, 512);
    CHECK (k2 > 0 && n2 > 0 && n2 % 4 == 0);
    CHECK (n2 <= 4 * (k2 / 2 + 1)); /* one flush per epoch, not per partial */
    dll_destroy (s2);

    /* Blobs zero the attachment (deterministic) and set_state into an
     * attached instance preserves that instance's live attachment. */
    size_t sb = dll_state_bytes (d);
    void  *b1 = malloc (sb), *b2 = malloc (sb);
    dll_get_state (d, b1);
    dll_state_t *d3 = dll_create (code, 31, 2, 0.0, 0.01, 0.707, 0.5, 1);
    CHECK (d3 != NULL);
    CHECK (dll_set_telemetry (d3, tlm, "code3", 2) == DP_OK);
    CHECK (dll_set_state (d3, b1) == DP_OK);
    CHECK (d3->tlm.ctx == tlm);
    CHECK (d3->tlm.id_e == dp_tlm_lookup (tlm, "code3.e"));
    dll_get_state (d3, b2);
    CHECK (memcmp (b1, b2, sb) == 0); /* attachment-independent bytes */
    free (b1);
    free (b2);
    dll_destroy (d3);

    /* Detach: probe sites revert to the single-branch cost. */
    CHECK (dll_set_telemetry (d, NULL, "code", 1) == DP_OK);
    CHECK (d->tlm.ctx == NULL);
    (void)dll_steps (d, rx, L, out, 256);
    CHECK (dp_tlm_read (tlm, recs, 512) == 0);

    /* A full probe table fails the attach whole. */
    char pname[DP_TLM_NAME_MAX];
    for (size_t i = 0; dp_tlm_probe_count (tlm) < DP_TLM_MAX_PROBES; i++)
      {
        (void)snprintf (pname, sizeof (pname), "fill%zu", i);
        (void)dp_tlm_probe (tlm, pname, 1);
      }
    CHECK (dll_set_telemetry (d, tlm, "nope", 1) == DP_ERR_INVALID);
    CHECK (d->tlm.ctx == NULL);
    dll_destroy (d);
    dp_tlm_destroy (tlm);
  }

  /* ---------------------------------------------------------------- *
   * 8. Long-run false-lock regression: zero Doppler, zero carrier,   *
   *    segments=1 -- the code phase must never make a large jump     *
   *    once locked. This is the committed guard for a real bug: an   *
   *    earlier fixed-point-NCO redesign attempt (dwell-integrated    *
   *    replica + magnitude discriminator, direct phase kicks) passed  *
   *    every other test in this file yet still false-locked into a   *
   *    stable, wrong code phase over thousands of periods on select   *
   *    noise seeds -- with no test here to catch it. The 2x-          *
   *    oversampled replica + power-domain NELP discriminator (see     *
   *    dll_replica()/dll_update()) is what actually fixed it; this    *
   *    test is what would have caught the earlier attempt's failure   *
   *    immediately instead of needing a multi-session investigation. *
   * ---------------------------------------------------------------- */
  {
    const size_t   sf = 63, sps = 4, nper = 4188, tsamps = sf * sps;
    const uint32_t seeds[] = { 42, 43, 44, 100 };
    const size_t   n_seeds = sizeof (seeds) / sizeof (seeds[0]);
    uint8_t       *code    = malloc (sf);
    float complex *rx      = malloc (tsamps * nper * sizeof (*rx));
    for (size_t si = 0; si < n_seeds; si++)
      {
        make_code (code, sf, 7u + seeds[si]);
        size_t n
            = make_signal (rx, code, sf, sps, 0.0, nper, 1000u + seeds[si], 1);
        dll_state_t *d = dll_create (code, sf, sps, 0.0, 0.02, 0.707, 0.5, 1);
        double       prev_phase = -1.0;
        double       max_jump   = 0.0;
        for (size_t p = 0; p < nper; p++)
          {
            for (size_t i = 0; i < tsamps; i++)
              dll_accumulate (d, rx[p * tsamps + i]);
            dll_update (d);
            d->acc_e = d->acc_p = d->acc_l = 0.0f;
            double cp                      = dll_get_code_phase (d);
            if (prev_phase >= 0.0)
              {
                double jump = fabs (cp - prev_phase);
                if (jump > (double)sf / 2.0)
                  jump = (double)sf - jump;
                if (jump > max_jump)
                  max_jump = jump;
              }
            prev_phase = cp;
          }
        (void)n;
        CHECK (max_jump < 5.0); /* no false-lock jump over the whole run */
        CHECK (fabs (dll_get_code_rate (d) - 1.0) < 1e-3);
        dll_destroy (d);
      }
    free (rx);
    free (code);
  }

  /* dll_lookback_segments(): ports despreader_coupled.py's
   * async_lookback_windows() -- the known reference value it derives at
   * this project's own validated point (tsamps=2046, max_error_db=0.5
   * -> windows=11). */
  {
    CHECK (dll_lookback_segments (2046, 0.5) == 11);
    /* tsamps==0 is a degenerate guard, not a real caller input. */
    CHECK (dll_lookback_segments (0, 0.5) == 1);
    /* Every returned segments count must evenly divide tsamps -- the
     * whole point of the divisor-snapping step. */
    size_t tsamps_probe[] = { 2046, 1024, 63 * 4, 31 * 2, 100 };
    for (size_t i = 0; i < sizeof (tsamps_probe) / sizeof (tsamps_probe[0]);
         i++)
      {
        size_t t = tsamps_probe[i];
        size_t s = dll_lookback_segments (t, 0.5);
        CHECK (s >= 1 && s <= t);
        CHECK (t % s == 0);
      }
  }

  if (_fails)
    {
      fprintf (stderr, "test_dll_core FAILED (%d)\n", _fails);
      return 1;
    }

  printf ("test_dll_core PASSED\n");
  return 0;
}
