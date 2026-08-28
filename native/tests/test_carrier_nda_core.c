/**
 * @file test_carrier_nda_core.c
 * @brief Unit tests for the non-data-aided M-th-power carrier loop.
 *
 * Tests:
 *   1. Lifecycle / param validation / init==create parity
 *   2. Arm moving-average cadence — one output/sample, boxcar window sum
 *   3. The M-th-power discriminator: phase_error = scaled Im(z^M) (zero with
 *      positive slope at lock; period-2pi/M sawtooth), lock = scaled Re(z^M)
 *   4. Cold-start pull-in on an UNMODULATED carrier (no data)
 *   5. Cold-start pull-in on MODULATED data with NO symbol timing (the
 * headline)
 *   6. Reset reproducibility
 *      — serialized state: whole-struct snapshot resumes bit-for-bit
 *   7. bn is n-invariant: settling does not scale with the arm dumps
 *   8. set_bn reconfigures the loop filter
 *   9. Amplitude invariance, at any scale and every M (there is no AGC)
 *      — telemetry: four sample-rate probes, attachment-independent blobs
 *  14. The lock signal is BOUNDED in +-1, at every M and every input
 *  15. The derived threshold chain, as arithmetic
 *  16. The SEEDING RULE: |df| <= bn/M settles within 2/bn samples
 *
 * This list was stale at "1-6" while the file carried sixteen sections —
 * the file's own index is a claim like any other.
 */
#include "carrier_nda/carrier_nda_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include "mpsk/mpsk_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TWOPI 6.283185307179586

/* Run the loop over a built signal; report tracked freq + lock. */
static void
run (carrier_nda_state_t *c, const float complex *rx, size_t n, double *f,
     double *lk)
{
  float complex *o = malloc (n * sizeof (*o));
  carrier_nda_steps (c, rx, n, o, n);
  *f  = carrier_nda_get_norm_freq (c);
  *lk = carrier_nda_get_lock (c);
  free (o);
}

/* Scan a cold-start run for the sample at which the tracked frequency
 * settles on f0, probing every `step` samples.
 *
 * `last` picks the semantics, and the two are NOT the same number: 0 gives
 * the FIRST arrival inside the +-tol_frac band, 1 gives the LAST excursion
 * out of it. They differ exactly when the loop overshoots, which a
 * zeta = 0.707 type-2 loop does — and a first-arrival measure then reports
 * the overshoot's outbound crossing as the answer. Section 7 wants the
 * cheap proxy (it compares a ratio across n); section 16 pins an absolute
 * budget a caller is told to rely on, so it takes the honest one.
 *
 * Returns N when the loop never settles inside the record. */
static size_t
track_settle (size_t sps, int n, int m, double bn, double f0,
              const float complex *rx, size_t N, double tol_frac, size_t step,
              int last)
{
  carrier_nda_state_t *c     = carrier_nda_create (bn, 0.707, 0.0, sps, n, m);
  float complex       *o     = malloc (step * sizeof (*o));
  size_t               first = N, out_at = 0;
  int                  ever_in = 0;
  for (size_t i = 0; i + step <= N; i += step)
    {
      carrier_nda_steps (c, rx + i, step, o, step);
      int in = fabs (carrier_nda_get_norm_freq (c) - f0) < tol_frac * f0;
      if (in && first == N)
        first = i;
      if (in)
        ever_in = 1;
      else
        out_at = i + step;
    }
  carrier_nda_destroy (c);
  free (o);
  if (!ever_in)
    return N; /* never arrived at all */
  return last ? out_at : first;
}

/* Section 7's proxy, unchanged: first arrival inside 10%, probed every 50
 * samples, at the shipped sps and M = 4. */
static size_t
settle_idx (int n, double bn, double f0, const float complex *rx, size_t N)
{
  return track_settle (8, n, 4, bn, f0, rx, N, 0.1, 50, 0);
}

int
main (void)
{

  /* ---------------------------------------------------------------- *
   * 1. Lifecycle, param validation, init==create parity              *
   * ---------------------------------------------------------------- */
  {
    carrier_nda_state_t *c = carrier_nda_create (0.01, 0.707, 0.01, 8, 4, 4);
    DP_CHECK (c != NULL);
    if (!c)
      return 1;
    DP_CHECK (c->lf.kp > 0.0 && c->lf.ki > 0.0);
    DP_CHECK (fabs (carrier_nda_get_norm_freq (c) - 0.01) < 1e-12);
    DP_CHECK (carrier_nda_get_m (c) == 4);
    DP_CHECK (carrier_nda_get_n (c) == 4);
    DP_CHECK (carrier_nda_get_sps (c) == 8);
    DP_CHECK (c->arm_len == 2); /* sps/n = 8/4 */

    carrier_nda_state_t v;
    carrier_nda_init (&v, 0.01, 0.707, 0.01, 8, 4, 4);
    DP_CHECK (v.lf.kp == c->lf.kp && v.lf.ki == c->lf.ki);
    DP_CHECK (v.nco.phase_inc == c->nco.phase_inc);
    DP_CHECK (v.arm_len == c->arm_len && v.m == c->m);
    carrier_nda_destroy (c);

    /* M in {2,4,8}; sps % n == 0; n > 0; sps > 0 */
    /* The ACCEPTING cases hand back a state, so the test that proves
       they are accepted also has to release it; only the rejecting
       ones below have nothing to free. */
    carrier_nda_state_t *ok;
    ok = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 2);
    DP_CHECK (ok != NULL);
    carrier_nda_destroy (ok);
    ok = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 8);
    DP_CHECK (ok != NULL);
    carrier_nda_destroy (ok);
    DP_CHECK (carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 3) == NULL);
    DP_CHECK (carrier_nda_create (0.01, 0.707, 0.0, 8, 3, 4)
              == NULL); /* 8%3 */
    DP_CHECK (carrier_nda_create (0.01, 0.707, 0.0, 8, 0, 4) == NULL);
    DP_CHECK (carrier_nda_create (0.01, 0.707, 0.0, 0, 4, 4) == NULL);
  }

  /* ---------------------------------------------------------------- *
   * 2. Arm moving-average: one output per input sample (no rate       *
   *    change), and the running window holds a boxcar sum of the last  *
   *    arm_len samples.                                                *
   * ---------------------------------------------------------------- */
  {
    int                  sps = 8, n = 4; /* arm_len = sps/n = 2 */
    carrier_nda_state_t *c = carrier_nda_create (0.01, 0.707, 0.0, sps, n, 4);
    int                  outs = 0;
    for (int i = 0; i < sps; i++) /* ramp 1..8 through the boxcar */
      {
        double pe, lk;
        if (carrier_nda_arm_step (c, (float)(i + 1) + 0.0f * I, &pe, &lk))
          outs++;
      }
    DP_CHECK (outs == sps); /* one output per input sample (no decimation) */
    /* boxcar window = last arm_len=2 samples: 7 + 8 = 15 (running sum) */
    DP_CHECK (fabs (crealf (c->arm.acc) - 15.0f) < 1e-4);
    DP_CHECK (fabs (cimagf (c->arm.acc)) < 1e-6);
    carrier_nda_destroy (c);

    /* arm_len > BOXCAR_MAX_LEN is rejected (fixed in-struct ring) */
    DP_CHECK (carrier_nda_create (0.01, 0.707, 0.0, 128, 1, 4) == NULL);
    carrier_nda_state_t *cmax
        = carrier_nda_create (0.01, 0.707, 0.0, BOXCAR_MAX_LEN, 1, 4);
    DP_CHECK (cmax != NULL); /* arm_len == BOXCAR_MAX_LEN is allowed */
    carrier_nda_destroy (cmax);
  }

  /* ---------------------------------------------------------------- *
   * 3. The M-th-power discriminator characteristic                   *
   * ---------------------------------------------------------------- */
  {
    for (int mi = 0; mi < 3; mi++)
      {
        int    m   = (mi == 0) ? 2 : (mi == 1) ? 4 : 8;
        double seg = TWOPI / m;
        double pe0, lk0;
        carrier_nda_disc (1.0f + 0.0f * I, m, &pe0, &lk0);
        DP_CHECK (fabs (pe0) < 1e-9); /* e(0) = 0          */
        DP_CHECK (lk0 > 0.0);         /* lock peaks at 0   */
        /* constant-gain property: phase_error slope at 0 is ~2 for all M */
        double h = 1e-3 / m, peh, pemh, lk;
        carrier_nda_disc ((float complex)cexp (I * h), m, &peh, &lk);
        carrier_nda_disc ((float complex)cexp (-I * h), m, &pemh, &lk);
        double slope = (peh - pemh) / (2.0 * h);
        DP_CHECK (fabs (slope - 2.0) < 2e-2);
        /* sawtooth period 2pi/M: e(phi) == e(phi + 2pi/M) */
        double pa, pb;
        carrier_nda_disc ((float complex)cexp (I * 0.05), m, &pa, &lk);
        carrier_nda_disc ((float complex)cexp (I * (0.05 + seg)), m, &pb, &lk);
        DP_CHECK (fabs (pa - pb) < 1e-6);
      }
  }

  /* ---------------------------------------------------------------- *
   * 4. Cold-start pull-in on an UNMODULATED carrier (no data)        *
   * ---------------------------------------------------------------- */
  {
    size_t         N    = 40000;
    float complex *rx   = malloc (N * sizeof (*rx));
    double         f0   = 0.001;
    int            ms[] = { 2, 4, 8 };
    for (int mi = 0; mi < 3; mi++)
      {
        uint32_t ns = 5u;
        for (size_t k = 0; k < N; k++)
          {
            /* Sequenced: two draws in one expression are indeterminately
               sequenced, and gcc takes the imaginary operand first while
               clang takes the real one. gcc's order is pinned. */
            float n_im = 0.05f * (float)dp_gauss (&ns);
            float n_re = 0.05f * (float)dp_gauss (&ns);
            rx[k] = (float complex)cexp (I * TWOPI * f0 * (double)k) + n_re
                    + n_im * I;
          }
        carrier_nda_state_t *c
            = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, ms[mi]);
        double f, lk;
        run (c, rx, N, &f, &lk);
        DP_CHECK (fabs (f - f0) < 5e-4); /* acquired the bare carrier */
        DP_CHECK (lk > 0.3);             /* locked (normalised: ~1)   */
        carrier_nda_destroy (c);
      }
    free (rx);
  }

  /* ---------------------------------------------------------------- *
   * 5. Cold-start on MODULATED data with NO symbol timing            *
   * ---------------------------------------------------------------- */
  {
    int            sps  = 8;
    size_t         nsym = 6000, N = nsym * (size_t)sps;
    float complex *rx   = malloc (N * sizeof (*rx));
    double         f0   = 0.001;
    int            ms[] = { 2, 4, 8 };
    for (int mi = 0; mi < 3; mi++)
      {
        int      m  = ms[mi];
        uint32_t ds = 99u, ns = 7u;
        for (size_t s = 0; s < nsym; s++)
          {
            float complex a
                = mpsk_constellation ((int)(dp_xs32 (&ds) % (uint32_t)m), m);
            for (int i = 0; i < sps; i++)
              {
                size_t k    = s * (size_t)sps + (size_t)i;
                float  n_im = 0.1f * (float)dp_gauss (&ns); /* gcc's order */
                float  n_re = 0.1f * (float)dp_gauss (&ns);
                rx[k] = a * (float complex)cexp (I * TWOPI * f0 * (double)k)
                        + n_re + n_im * I;
              }
          }
        carrier_nda_state_t *c
            = carrier_nda_create (0.01, 0.707, 0.0, sps, 4, m);
        double f, lk;
        run (c, rx, N, &f, &lk);
        DP_CHECK (fabs (f - f0) < 5e-4); /* locked despite NO timing  */
        DP_CHECK (lk > 0.3);
        carrier_nda_destroy (c);
      }
    free (rx);
  }

  /* ---------------------------------------------------------------- *
   * 6. Reset reproducibility                                         *
   * ---------------------------------------------------------------- */
  {
    size_t         N  = 8000;
    float complex *rx = malloc (N * sizeof (*rx));
    uint32_t       ns = 3u;
    for (size_t k = 0; k < N; k++)
      {
        float n_im = 0.05f * (float)dp_gauss (&ns); /* gcc's order */
        float n_re = 0.05f * (float)dp_gauss (&ns);
        rx[k] = (float complex)cexp (I * TWOPI * 0.0012 * (double)k) + n_re
                + n_im * I;
      }
    carrier_nda_state_t *c = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 4);
    double               f1, lk1, f2, lk2;
    run (c, rx, N, &f1, &lk1);
    carrier_nda_reset (c);
    run (c, rx, N, &f2, &lk2);
    DP_CHECK (f1 == f2 && lk1 == lk2);
    carrier_nda_destroy (c);
    free (rx);
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

    carrier_nda_state_t *a  = carrier_nda_create (0.01, 0.707, 0.0, 4, 2, 4);
    size_t               nA = carrier_nda_steps (a, rx, L, outA, CAP);
    carrier_nda_destroy (a);

    carrier_nda_state_t *r1   = carrier_nda_create (0.01, 0.707, 0.0, 4, 2, 4);
    size_t               nB   = carrier_nda_steps (r1, rx, CUT, outB, CAP);
    size_t               sb   = carrier_nda_state_bytes (r1);
    void                *blob = malloc (sb);
    carrier_nda_get_state (r1, blob);
    carrier_nda_destroy (r1);

    carrier_nda_state_t *r2 = carrier_nda_create (0.01, 0.707, 0.0, 4, 2, 4);
    DP_CHECK (carrier_nda_set_state (r2, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF;
    DP_CHECK (carrier_nda_set_state (r2, blob) == DP_ERR_INVALID);
    ((char *)blob)[0] ^= (char)0xFF;
    nB += carrier_nda_steps (r2, rx + CUT, L - CUT, outB + nB, CAP - nB);
    carrier_nda_destroy (r2);
    free (blob);

    DP_CHECK (nA == nB);
    for (size_t i = 0; i < nA && i < nB; i++)
      DP_CHECK (crealf (outA[i]) == crealf (outB[i])
                && cimagf (outA[i]) == cimagf (outB[i]));
    free (rx);
    free (outA);
    free (outB);
  }

  /* ---------------------------------------------------------------- *
   * 7. Bn is n-invariant: at a fixed bn the closed-loop settling time *
   *    is ~independent of n (arm dumps/symbol). Before the fix bn was  *
   *    applied per arm-dump, so the real-time loop bandwidth — and     *
   *    settling — scaled ~n x. Noiseless carrier → deterministic.      *
   * ---------------------------------------------------------------- */
  {
    size_t         N  = 40000;
    float complex *rx = malloc (N * sizeof (*rx));
    double         f0 = 0.0015;
    for (size_t k = 0; k < N; k++)
      rx[k] = (float complex)cexp (I * TWOPI * f0 * (double)k);
    size_t s1 = settle_idx (1, 0.005, f0, rx, N);
    size_t s2 = settle_idx (2, 0.005, f0, rx, N);
    size_t s4 = settle_idx (4, 0.005, f0, rx, N);
    DP_CHECK (s1 < N && s2 < N && s4 < N); /* all settle */
    double smin
        = (double)(s1 < s2 ? (s1 < s4 ? s1 : s4) : (s2 < s4 ? s2 : s4));
    double smax
        = (double)(s1 > s2 ? (s1 > s4 ? s1 : s4) : (s2 > s4 ? s2 : s4));
    /* n-invariant: settling within ~2x across n (would be ~4x apart with
     * the old per-dump bn). */
    DP_CHECK ((smax + 50.0) / (smin + 50.0) < 2.5);
    free (rx);
  }

  /* ---------------------------------------------------------------- *
   * 8. set_bn reconfigures the loop filter.                           *
   * ---------------------------------------------------------------- */
  {
    carrier_nda_state_t *c   = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 4);
    double               kp0 = c->lf.kp;
    carrier_nda_set_bn (c, 0.02);
    DP_CHECK (carrier_nda_get_bn (c) == 0.02);
    DP_CHECK (c->lf.kp != kp0);
    carrier_nda_destroy (c);
  }

  /* ---------------------------------------------------------------- *
   * 9. Amplitude invariance, at ANY scale and at every M.             *
   *                                                                    *
   *    There is no AGC in this loop; carrier_nda_disc divides out its   *
   *    own |z|^M, so input scale must not reach the loop at all. This   *
   *    is a range gate as much as a gain gate: forming |z|^M explicitly *
   *    (which the pre-gh-657 form did, because an arm AGC pinned |z|=1  *
   *    and hid it) dies at BOTH ends in float — M=8 returned exactly 0  *
   *    below |z|=0.032, where the eps guard on |z|^8 trips, and NaN     *
   *    above |z|=1e4, where |z|^8 passes FLT_MAX. The scales below      *
   *    straddle both, so this fails if the divide is ever un-hoisted.   *
   * ---------------------------------------------------------------- */
  {
    /* Discriminator level first: exact, and it localises a failure to the
     * detector rather than to the loop wrapped around it. */
    const int    ms[] = { 2, 4, 8 };
    const double th   = 0.11; /* off lock, so pe and lock are both nonzero */
    for (int mi = 0; mi < 3; mi++)
      {
        double        ref_pe, ref_lk;
        float complex z1 = (float)cos (th) + (float)sin (th) * I;
        carrier_nda_disc (z1, ms[mi], &ref_pe, &ref_lk);
        DP_CHECK (fabs (ref_pe) > 0.1 && fabs (ref_lk) > 0.1);
        const double amps[] = { 1e-5, 1e-3, 3.2e-2, 1.0, 1e3, 1e5, 1e8 };
        for (size_t ai = 0; ai < sizeof amps / sizeof *amps; ai++)
          {
            double        A = amps[ai];
            float complex z
                = (float)(A * cos (th)) + (float)(A * sin (th)) * I;
            double pe, lk;
            carrier_nda_disc (z, ms[mi], &pe, &lk);
            DP_CHECK (fabs (pe - ref_pe) <= 1e-5 * fabs (ref_pe));
            DP_CHECK (fabs (lk - ref_lk) <= 1e-5 * fabs (ref_lk));
          }
        /* Degenerate input yields zero, never a NaN into the loop filter. */
        double pe0, lk0;
        carrier_nda_disc (0.0f + 0.0f * I, ms[mi], &pe0, &lk0);
        DP_CHECK (pe0 == 0.0 && lk0 == 0.0);
      }
  }
  {
    size_t         N  = 40000;
    float complex *rx = malloc (N * sizeof (*rx));
    double         f0 = 0.001;
    uint32_t       ns = 23u;
    for (size_t k = 0; k < N; k++)
      {
        float n_im = 0.05f * (float)dp_gauss (&ns); /* gcc's order */
        float n_re = 0.05f * (float)dp_gauss (&ns);
        rx[k]      = (float complex)cexp (I * TWOPI * f0 * (double)k) + n_re
                     + n_im * I;
      }
    /* The DISCRIMINATOR's own invariance, which is the claim the loop test
     * below only exercises indirectly. The header states both outputs are
     * "invariant to input scale over the whole float range: measured
     * identical to 1e-6 relative from an amplitude of 1e-5 to 1e15, at
     * every M" -- twenty decades, where the loop sweep below covers eight
     * and reads a converged frequency rather than the detector. The header
     * said 5e-7 until this test measured it: the true worst case is 6.5e-7,
     * at M = 8 and a scale of 1e15, which is ~5 float eps and exactly the
     * rounding floor a float detector has. Substance unchanged, tolerance
     * corrected to what holds.
     *
     * This is the claim that retired the arm AGC, so it is worth pinning as
     * stated. It is also the regression test for the hoisted divide: forming
     * |z|^M at the END overflows float in both directions at M = 8, which
     * returned exactly zero below |z| = 0.032 and NaN above |z| = 1e5 --
     * both inside the range asserted here, and both unreachable while an AGC
     * upstream manufactured |z| ~ 1. */
    {
      const double sc[] = { 1e-5, 1e-2, 1.0, 1e2, 1e8, 1e15 };
      const int    ms[] = { 2, 4, 8 };
      for (size_t mi = 0; mi < sizeof ms / sizeof *ms; mi++)
        {
          /* An off-axis phase, so both outputs are non-trivial and a
             relative comparison means something. */
          float complex z0 = (float)cos (0.3) + (float)sin (0.3) * I;
          double        pe0, lk0;
          carrier_nda_disc (z0, ms[mi], &pe0, &lk0);
          DP_CHECK (fabs (pe0) > 1e-3 && fabs (lk0) > 1e-3);
          for (size_t si = 0; si < sizeof sc / sizeof *sc; si++)
            {
              double pe, lk;
              carrier_nda_disc ((float complex) ((float)sc[si]) * z0, ms[mi],
                                &pe, &lk);
              DP_CHECK (isfinite (pe) && isfinite (lk));
              DP_CHECK (fabs (pe - pe0) <= 1e-6 * fabs (pe0));
              DP_CHECK (fabs (lk - lk0) <= 1e-6 * fabs (lk0));
            }
        }
    }

    /* 1e-4 .. 1e4 — eight decades, centred on the unit scale the retired
     * AGC used to be the only way to reach. */
    double scales[] = { 1e-4, 1e-2, 1.0, 1e2, 1e4 };
    enum
    {
      NS = sizeof scales / sizeof *scales
    };
    float complex *rs = malloc (N * sizeof (*rs));
    for (int m = 4; m <= 8; m += 4) /* QPSK and the 8PSK worst case */
      {
        double fs[NS];
        for (int si = 0; si < NS; si++)
          {
            for (size_t k = 0; k < N; k++)
              rs[k] = (float)scales[si] * rx[k];
            carrier_nda_state_t *c
                = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, m);
            double f, lk;
            run (c, rs, N, &f, &lk);
            DP_CHECK (fabs (f - f0) < 5e-4); /* converges at any scale */
            DP_CHECK (lk > 0.3);             /* lock metric level-invariant */
            fs[si] = f;
            carrier_nda_destroy (c);
          }
        for (int si = 0; si < NS; si++)
          DP_CHECK (fabs (fs[si] - fs[NS / 2]) < 1e-4); /* same carrier */
      }
    free (rs);
    free (rx);
  }

  /* telemetry attach — four sample-rate probes; blobs stay
   * attachment-independent (the attachment is zeroed in the bytes). */
  {
    enum
    {
      N = 1024
    };
    float complex rx[N], out[N];
    dp_tlm_rec_t  recs[8192];
    for (int i = 0; i < N; i++)
      rx[i] = (float complex)cexp (I * TWOPI * 0.005 * (double)i);
    dp_tlm_t            *tlm = dp_tlm_create (1 << 13);
    carrier_nda_state_t *c   = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 4);
    DP_CHECK (tlm != NULL && c != NULL);
    DP_CHECK (carrier_nda_set_telemetry (c, tlm, "car", 1) == DP_OK);
    DP_CHECK (dp_tlm_probe_id (tlm, "car.lock") == c->tlm.id_lock);
    DP_CHECK (dp_tlm_probe_id (tlm, "car.e") == c->tlm.id_e);
    DP_CHECK (dp_tlm_probe_id (tlm, "car.freq") == c->tlm.id_freq);
    DP_CHECK (dp_tlm_probe_id (tlm, "car.locked") == c->tlm.id_locked);
    /* The retired arm AGC used to forward a fifth probe here (gh-657). */
    DP_CHECK (dp_tlm_probe_count (tlm) == 4);

    size_t k = carrier_nda_steps (c, rx, N, out, N);
    DP_CHECK (k == N);
    size_t n_rec = dp_tlm_read (tlm, 8192, recs, 8192);
    DP_CHECK (n_rec == 4 * N); /* four per sample, nothing else */
    /* Per-sample emit order is lock, e, freq, locked -- the last record is
     * the lockdet decision; the one before it mirrors the tracked carrier. */
    DP_CHECK (recs[n_rec - 1].probe == (uint16_t)c->tlm.id_locked);
    DP_CHECK (recs[n_rec - 1].value == (float)c->lockdet.locked);
    DP_CHECK (recs[n_rec - 2].value
              == (float)(c->nco.norm_freq + c->lf.integ));

    /* Blobs zero the attachment (deterministic) and set_state into an
     * attached instance preserves that instance's live attachment. */
    size_t sb = carrier_nda_state_bytes (c);
    void  *b1 = malloc (sb), *b2 = malloc (sb);
    carrier_nda_get_state (c, b1);
    carrier_nda_state_t *d = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 4);
    DP_CHECK (d != NULL);
    DP_CHECK (carrier_nda_set_telemetry (d, tlm, "car2", 4) == DP_OK);
    DP_CHECK (carrier_nda_set_state (d, b1) == DP_OK);
    DP_CHECK (d->tlm.ctx == tlm);
    DP_CHECK (d->tlm.id_e == dp_tlm_probe_id (tlm, "car2.e"));
    carrier_nda_get_state (d, b2);
    DP_CHECK (memcmp (b1, b2, sb) == 0); /* attachment-independent bytes */
    free (b1);
    free (b2);
    carrier_nda_destroy (d);

    DP_CHECK (carrier_nda_set_telemetry (c, NULL, "car", 1) == DP_OK);
    DP_CHECK (c->tlm.ctx == NULL);
    (void)carrier_nda_steps (c, rx, N, out, N);
    DP_CHECK (dp_tlm_read (tlm, 8192, recs, 8192) == 0);

    /* A full probe table fails the attach whole. */
    char pname[DP_TLM_NAME_MAX];
    for (size_t i = 0; dp_tlm_probe_count (tlm) < DP_TLM_MAX_PROBES; i++)
      {
        (void)snprintf (pname, sizeof (pname), "fill%zu", i);
        (void)dp_tlm_probe (tlm, pname, 1);
      }
    DP_CHECK (carrier_nda_set_telemetry (c, tlm, "nope", 1) == DP_ERR_INVALID);
    DP_CHECK (c->tlm.ctx == NULL);
    carrier_nda_destroy (c);
    dp_tlm_destroy (tlm);
  }

  /* ---------------------------------------------------------------- *
   * 14. The lock signal is BOUNDED in +-1, at every M and every input. *
   *                                                                    *
   *    Half of the argument that made this statistic a detector you    *
   *    can put a number on. The header's case for limiting is two      *
   *    properties: bounded, and M-independent under H0. The second is  *
   *    measured (carrier_nda_lock asserts mean 0 / var 1/2 at every    *
   *    M); the first was asserted in prose and by nothing else.        *
   *                                                                    *
   *    It is not implied by the H0 law. A statistic can have the right *
   *    variance and still excurse: the RAW form Re(z^M) has exactly    *
   *    this shape and, at M = 8 on Gaussian noise, an sd of 137 per    *
   *    look against a value of 1.0 at lock. Bounding is what the       *
   *    limiter buys, so it is what a regression would take away.       *
   *                                                                    *
   *    Swept over phase AND over the scales section 9 uses, because    *
   *    the bound has to survive the same range the divide does -- an   *
   *    un-hoisted |z|^M breaks boundedness long before it breaks       *
   *    finiteness.                                                     *
   * ---------------------------------------------------------------- */
  {
    const int ms[] = { 2, 4, 8 };
    /* Straddles the eps guard (3.2e-2) and the old FLT_MAX cliff (1e5). */
    const double amps[] = { 1e-5, 3.2e-2, 1.0, 1e5, 1e15 };
    for (int mi = 0; mi < 3; mi++)
      {
        double worst = 0.0;
        for (int k = 0; k < 512; k++)
          {
            /* A full period at this M, so every phase of the M-th power
               is visited including the peaks. */
            double th = -M_PI + 2.0 * M_PI * (double)k / 512.0;
            for (size_t ai = 0; ai < sizeof amps / sizeof *amps; ai++)
              {
                double        A = amps[ai];
                float complex z
                    = (float)(A * cos (th)) + (float)(A * sin (th)) * I;
                double pe, lk;
                carrier_nda_disc (z, ms[mi], &pe, &lk);
                DP_CHECK (isfinite (lk) && isfinite (pe));
                DP_CHECK (fabs (lk) <= 1.0 + 1e-6);
                if (fabs (lk) > worst)
                  worst = fabs (lk);
              }
          }
        /* Precondition, so the bound is not passing vacuously: a lock
           signal pinned at zero would satisfy every check above. The
           sweep covers a full period, so it must REACH the bound. */
        DP_CHECK (worst > 0.99);
      }
  }

  /* ---------------------------------------------------------------- *
   * 15. The derived threshold chain, as arithmetic.                    *
   *                                                                    *
   *    The header derives three numbers rather than picking them:      *
   *      alpha  = 0.05                  -> N_eff = (2-a)/a = 39 looks  *
   *      sd_H0  = sqrt(1/2 * a/(2-a))   = 0.1132                       *
   *      thresh = eta * sd_H0, and the shipped default 0.5 is          *
   *               eta = 4.416, a per-look Pfa of 5e-6                  *
   *                                                                    *
   *    All three reach a caller as macros, and nothing checked that    *
   *    the macros still satisfy the identities they were derived from. *
   *    This is pure arithmetic -- no Monte-Carlo, no seeds -- so it    *
   *    belongs here rather than in a sweep, and it is exactly the      *
   *    class of claim that rots silently when a constant is retuned:   *
   *    change CARRIER_NDA_LOCK_ALPHA alone and the sd macro beside it  *
   *    becomes a number with no derivation behind it.                  *
   * ---------------------------------------------------------------- */
  {
    const double a = CARRIER_NDA_LOCK_ALPHA;
    /* N_eff = (2-a)/a, the effective look count the alpha was chosen for
       (>= the 30-look floor the header cites). */
    const double n_eff = (2.0 - a) / a;
    DP_CHECK (fabs (n_eff - 39.0) < 0.5);

    /* Var per look is 1/2 EXACTLY at every M (H0, theta uniform), so the
       post-EMA sd follows in closed form. The macro must BE this. */
    const double sd = sqrt (0.5 * a / (2.0 - a));
    DP_CHECK (fabs (sd - CARRIER_NDA_LOCK_NORM_SD) < 1e-12);

    /* The shipped up-threshold, read back as its Pfa multiplier. 0.5 was
       a long-standing default that turned out to BE the Pfa-derived value
       once the statistic became M-independent; that coincidence is the
       claim, so it is what gets pinned.

       Read off a CONSTRUCTED object rather than from the macro the header
       names. `CARRIER_NDA_LOCK_DEFAULT_UP` is defined in carrier_nda_core.c
       and is invisible from the header, so the header's derivation cites a
       symbol its own reader cannot reach -- and a test that hard-coded 0.5
       would not notice create() installing something else. What matters to
       a caller is the threshold the object actually runs with. */
    carrier_nda_state_t *cd = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 4);
    DP_CHECK (cd != NULL);
    const double eta = cd->lockdet.up_thresh / CARRIER_NDA_LOCK_NORM_SD;
    DP_CHECK (fabs (eta - 4.416) < 5e-3);
    carrier_nda_destroy (cd);
  }

  /* ---------------------------------------------------------------- *
   * 16. The SEEDING RULE — the number a composer sizes its coarse      *
   *     acquisition to.                                                *
   *                                                                    *
   *    This loop is meant to be handed a residual, not to find one by  *
   *    grinding, so the contract a caller needs is "how close must the  *
   *    seed be, and how long until it is locked". The validation report *
   *    (src/doppler/track/tests/validation/carrier_nda, section 2.5)    *
   *    measured it across three M and three bn:                         *
   *                                                                    *
   *      seed within |df| <= bn/M  ->  settled within 2/bn samples      *
   *                                                                    *
   *    and found the behaviour splits into two regimes about that       *
   *    point. Normalising the offset as u = |df|*M/bn and the time as   *
   *    T*bn, nine (M, bn) pairs collapse onto ONE curve: flat at        *
   *    1.76-1.92 loop constants for u <= 1, then quadratic.             *
   *                                                                    *
   *    Three separate claims, because they fail in different ways:      *
   *      (a) the BUDGET holds at the window edge, at every M;           *
   *      (b) inside the window settling does not depend on the offset   *
   *          at all — a linear 2nd-order loop settles in a fixed number *
   *          of time constants whatever the step size. This is the      *
   *          "predictable" the rule is worth having: constant, not      *
   *          merely bounded;                                            *
   *      (c) outside it, it is far slower — which is what stops (a)     *
   *          passing vacuously. An accessor wired to "settled" would    *
   *          satisfy (a) and (b) and fail (c).                          *
   *    And across M, since the whole point of the bn/M form is that M   *
   *    scales out.                                                      *
   * ---------------------------------------------------------------- */
  {
    enum
    {
      NS16 = 40000
    };
    const int    ms[] = { 2, 4, 8 };
    const double bn   = 0.01;
    const size_t bud  = (size_t)(2.5 / bn); /* the 2/bn rule, with margin
                                               for platform variation --
                                               measured worst case 1.92 */
    float complex *rx = malloc (NS16 * sizeof (*rx));
    size_t         s_in[3], s_edge[3], s_out[3];
    for (int mi = 0; mi < 3; mi++)
      {
        const int m = ms[mi];
        /* u = 0.25, 1.0 (the window edge) and 4.0 (well outside it). */
        const double us[3]  = { 0.25, 1.0, 4.0 };
        size_t      *dst[3] = { &s_in[mi], &s_edge[mi], &s_out[mi] };
        for (int ui = 0; ui < 3; ui++)
          {
            double f0 = us[ui] * bn / (double)m;
            for (size_t k = 0; k < NS16; k++)
              rx[k] = (float complex)cexp (I * TWOPI * f0 * (double)k);
            /* Noiseless, so the tolerance is purely relative: an absolute
               floor would be a different criterion at each M, because f0
               scales as 1/M. That error cost the report a wrong result
               before a limit caught it. */
            *dst[ui] = track_settle (8, 4, m, bn, f0, rx, NS16, 0.05, 8, 1);
          }
        /* (a) the budget, at the window edge. */
        DP_CHECK (s_edge[mi] <= bud);
        /* (b) offset-independent inside the window: a quarter of the way
               in costs the same as the edge, within the probe's own
               resolution plus a little. */
        DP_CHECK (s_in[mi] <= bud);
        DP_CHECK (fabs ((double)s_edge[mi] - (double)s_in[mi])
                  <= 0.25 * (double)bud);
        /* (c) and outside it, far slower — the regime boundary is real. */
        DP_CHECK (s_out[mi] > 4 * s_edge[mi]);
      }
    /* M scales out: that is what makes bn/M the form of the rule rather
       than a per-order table. */
    for (int mi = 1; mi < 3; mi++)
      DP_CHECK (fabs ((double)s_edge[mi] - (double)s_edge[0])
                <= 0.25 * (double)s_edge[0]);
    free (rx);
  }

  DP_TEST_END ("test_carrier_nda_core");
}
