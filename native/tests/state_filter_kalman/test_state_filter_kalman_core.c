/**
 * @file test_state_filter_kalman_core.c
 * @brief Unit tests for the Kalman flavor of the third-order state filter.
 *
 * Sections: the steady state against the Riccati equation itself, the ratio
 * that selects a bandwidth and the domain, the automatic case and the larger
 * start, the blob and its rejects, the caller's figures, the plain covariance
 * update at the slow end, and the binding's dispatch.
 */
#include "doppler/state_filter_kalman/state_filter_kalman_core.h"
#include "dp_chunk_inv.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The chunk-invariance harness's three hooks: a fresh filter, and `steps`
 * over a partition of the stream. The filter starts at the high-gain end of
 * the accepted range, so the covariance is moving across the partitions. */
static void *
k_create_obj (void *arg)
{
  (void)arg;
  return dp_state_filter_kalman_create (1.0, 2e-3, 1.0, 0.0);
}

static void
k_destroy_obj (void *o)
{
  dp_state_filter_kalman_destroy (o);
}

static size_t
k_process (void *o, const void *in, size_t n, void *out, size_t cap)
{
  (void)cap;
  dp_state_filter_kalman_steps (o, in, out, n);
  return n;
}

/* A reference for the covariance recursion in long double (wider than double
 * on x86-64 and aarch64 Linux; the same width where it is not, so there it
 * checks the recursion against a second copy of itself and the Riccati
 * residual test below is the independent one): the peak, over `n` updates from
 * the covariance `p0`, of the rate gain the filter holds. */
static long double
ref_peak_kr (const double p0[6], double q, size_t n)
{
  long double p[6];
  for (int i = 0; i < 6; i++)
    p[i] = (long double)p0[i];
  long double peak = 0.0L;
  for (size_t it = 0; it < n; it++)
    {
      const long double S  = p[0] + 1.0L;
      const long double a0 = p[0] + p[1], a1 = p[1] + p[3], a2 = p[2] + p[4];
      const long double b1 = p[3] + p[4], b2 = p[4] + p[5];
      const long double K0 = (p[0] + p[1]) / S, K1 = (p[1] + p[2]) / S,
                        K2 = p[2] / S;
      p[0]                 = a0 + a1 - S * K0 * K0;
      p[1]                 = a1 + a2 - S * K0 * K1;
      p[2]                 = a2 - S * K0 * K2;
      p[3]                 = b1 + b2 - S * K1 * K1;
      p[4]                 = b2 - S * K1 * K2;
      p[5]                 = p[5] - S * K2 * K2 + (long double)q;
      const long double kr = p[2] / (p[0] + 1.0L);
      if (kr > peak)
        peak = kr;
    }
  return peak;
}

/* The rule that bounds a start, checked at EVERY update and over a finer
 * rectangle than the constructor uses (the constructor samples the updates
 * and tests the four corners K in {0.5, 2} by delay in {0, 2}; this is the
 * reference it must agree with): the gains in force stay stable at delays 0,
 * 1 and 2 and loop gains 0.5, 0.75, 1, 1.5 and 2, for the 8/(bn*t) updates of
 * the covariance's trajectory. Written here from the public init and update,
 * sharing none of the constructor's sampling, corners or horizon formula. */
static int
dense_start_ok (double q, double start)
{
  dp_state_filter_kalman_state_t k;
  dp_state_filter_kalman_init_qr (&k, q, 1.0, 1.0, start);
  double p[6], g[3];
  DP_REQUIRE (dp_state_filter_kalman_steady (q, 1.0, p, g) == 1);
  const double bnt
      = dp_state_filter_loop_bandwidth (3, g[0], g[1], g[2], 0.0, 1.0);
  const size_t n = (size_t)(8.0 / bnt);
  for (size_t i = 0; i < n; i++)
    {
      static const double ks[5] = { 0.5, 0.75, 1.0, 1.5, 2.0 };
      static const double ds[3] = { 0.0, 1.0, 2.0 };
      for (int a = 0; a < 5; a++)
        for (int b = 0; b < 3; b++)
          if (!dp_state_filter_loop_stable (3, k.base.kp, k.base.ki, k.base.kr,
                                            ds[b], ks[a]))
            return 0;
      dp_state_filter_kalman_update (&k, 0.0);
    }
  return 1;
}

int
main (void)
{
  /* ── The Kalman flavor: the recursion's steady state ───────────────── */
  {
    /* Oracle: the steady-state covariance satisfies the Riccati equation
     * itself. Residual of P = F P F' - S K K' + Q, computed here from the
     * definition with full matrices, not from the library's packed form. */
    const double qs[] = { 1e-14, 1e-10, 1e-6, 1e-3, 1e-1 };
    for (size_t i = 0; i < sizeof qs / sizeof *qs; i++)
      {
        const double q = qs[i], r = 1.0;
        double       p6[6], g[3];
        DP_CHECK (dp_state_filter_kalman_steady (q, r, p6, g) == 1);
        double       P[3][3] = { { p6[0], p6[1], p6[2] },
                                 { p6[1], p6[3], p6[4] },
                                 { p6[2], p6[4], p6[5] } };
        const double F[3][3] = { { 1, 1, 0 }, { 0, 1, 1 }, { 0, 0, 1 } };
        double       FP[3][3], FPF[3][3];
        for (int a = 0; a < 3; a++)
          for (int b = 0; b < 3; b++)
            {
              FP[a][b] = 0;
              for (int c = 0; c < 3; c++)
                FP[a][b] += F[a][c] * P[c][b];
            }
        for (int a = 0; a < 3; a++)
          for (int b = 0; b < 3; b++)
            {
              FPF[a][b] = 0;
              for (int c = 0; c < 3; c++)
                FPF[a][b] += FP[a][c] * F[b][c];
            }
        const double S = P[0][0] + r;
        double       K[3];
        for (int a = 0; a < 3; a++)
          K[a] = FP[a][0] / S;
        double worst = 0, big = 0;
        for (int a = 0; a < 3; a++)
          for (int b = 0; b < 3; b++)
            {
              const double rhs
                  = FPF[a][b] - S * K[a] * K[b] + ((a == 2 && b == 2) ? q : 0);
              worst = fmax (worst, fabs (rhs - P[a][b]));
              big   = fmax (big, fabs (P[a][b]));
            }
        DP_CHECK (worst <= 1e-13 * big);
        /* The gains are the observer's K in the loop's coordinates. */
        DP_CHECK (fabs (g[2] - K[2]) <= 1e-15);
        DP_CHECK (fabs (g[1] - (K[1] - 2 * K[2])) <= 1e-14);
        DP_CHECK (fabs (g[0] - (K[0] - K[1] + K[2])) <= 1e-14);
      }
    DP_CHECK (dp_state_filter_kalman_steady (0.0, 1.0, (double[6]){ 0 },
                                             (double[3]){ 0 })
              == 0);
  }

  /* ── ... the ratio that selects a bandwidth, and the domain ──────────── */
  {
    for (double bnt = 1e-6; bnt <= 0.12; bnt *= 4.0)
      {
        const double ratio = dp_state_filter_kalman_ratio_for_bandwidth (bnt);
        DP_CHECK (ratio > 0.0);
        double p6[6], g[3];
        dp_state_filter_kalman_steady (ratio, 1.0, p6, g);
        const double bn
            = dp_state_filter_loop_bandwidth (3, g[0], g[1], g[2], 0.0, 1.0);
        DP_CHECK (fabs (bn - bnt) <= 1e-9 * bnt);
      }
    DP_CHECK (dp_state_filter_kalman_ratio_for_bandwidth (1e-7) == 0.0);
    DP_CHECK (dp_state_filter_kalman_ratio_for_bandwidth (0.2) == 0.0);
    const double q = dp_state_filter_kalman_ratio_for_bandwidth (0.04);
    DP_CHECK (dp_state_filter_kalman_params_ok_qr (q, 1.0, 1.0, 1.0));
    DP_CHECK (!dp_state_filter_kalman_params_ok_qr (q, 1.0, 1.0, 0.5));
    DP_CHECK (!dp_state_filter_kalman_params_ok_qr (
        q, 1.0, 1.0, STATE_FILTER_KALMAN_START_MAX + 0.1));
    DP_CHECK (!dp_state_filter_kalman_params_ok_qr (q, 0.0, 1.0, 1.0));
    DP_CHECK (!dp_state_filter_kalman_params_ok_qr (q, 1.0, 0.0, 1.0));
    DP_CHECK (!dp_state_filter_kalman_params_ok_qr (NAN, 1.0, 1.0, 1.0));
    DP_CHECK (dp_state_filter_kalman_create_qr (q, 0.0, 1.0, 1.0) == NULL);
  }

  /* ── ... the automatic case is a constant-gain loop; a larger start
   *    converges to it; reset returns to the start ─────────────────────── */
  {
    const double q = dp_state_filter_kalman_ratio_for_bandwidth (0.04);
    dp_state_filter_kalman_state_t *a
        = dp_state_filter_kalman_create_qr (q, 1.0, 1.0, 1.0);
    dp_state_filter_kalman_state_t *b
        = dp_state_filter_kalman_create_qr (q, 1.0, 1.0, 4.0);
    DP_CHECK (a && b);
    const double kp0 = a->base.kp, ki0 = a->base.ki, kr0 = a->base.kr;
    DP_CHECK (b->base.kr == kr0); /* update 0 reads P's first column only */
    for (int i = 0; i < 40; i++)
      dp_state_filter_kalman_step (a, 0.1);
    /* constant to rounding: the plain recursion's fixed point is the doubling
     * solve's to about 1e-15, so the gains drift at that level (an exact
     * equality here depends on the compiler's contraction); a recursion that
     * moved the gains would move them by orders of magnitude more */
    DP_CHECK (fabs (a->base.kp - kp0) <= 1e-12 * kp0
              && fabs (a->base.ki - ki0) <= 1e-12 * ki0
              && fabs (a->base.kr - kr0) <= 1e-12 * kr0);
    for (int i = 0; i < 20000; i++)
      dp_state_filter_kalman_step (b, 0.0);
    DP_CHECK (fabs (b->base.kr - kr0) <= 1e-9 * kr0);
    DP_CHECK (fabs (b->base.kp - kp0) <= 1e-9 * kp0);
    /* A larger start raises the gains for a while: they dip, climb to a
     * peak (measured about 1.4 times steady at start 4) and settle back to
     * steady state, which the 20000-update check above pins. */
    dp_state_filter_kalman_reset (b);
    double peak = 0.0;
    for (int i = 0; i < 600; i++)
      {
        dp_state_filter_kalman_step (b, 0.0);
        peak = fmax (peak, b->base.kr);
      }
    /* The peak is the trajectory's shape, checked against a recursion in
     * long double started from the same covariance (a different precision of
     * the same equations; the equations themselves are held by the Riccati
     * residual test above, taken from the definition with full matrices). The
     * peak is about 1.4 times the steady kr at start 4, bn*t 0.04. */
    {
      /* say which width this ran at: with long double no wider than
       * double the "reference" is the same arithmetic written twice, so the
       * pin below checks the recursion's transcription, not its rounding */
      printf ("peak pin: long double is %d bits (double is %d)%s\n",
              LDBL_MANT_DIG, DBL_MANT_DIG,
              LDBL_MANT_DIG > DBL_MANT_DIG
                  ? ""
                  : ": checks the transcription only");
      const long double ref = ref_peak_kr (b->p0, q, 600);
      DP_CHECK (fabsl ((long double)peak / ref - 1.0L) < 1e-9L);
      DP_CHECK (peak / kr0 > 1.3 && peak / kr0 < 1.5);
    }
    dp_state_filter_kalman_reset (b);
    DP_CHECK (b->base.integ == 0.0 && b->base.rate == 0.0);
    DP_CHECK (b->base.kr == kr0);
    /* steps == repeated step */
    double   x[64], o1[64], o2[64];
    uint32_t seed = 7;
    for (int i = 0; i < 64; i++)
      x[i] = (double)dp_xs32 (&seed) / 4294967296.0 - 0.5;
    dp_state_filter_kalman_reset (b);
    dp_state_filter_kalman_steps (b, x, o1, 64);
    dp_state_filter_kalman_reset (b);
    for (int i = 0; i < 64; i++)
      o2[i] = dp_state_filter_kalman_step (b, x[i]);
    DP_CHECK (memcmp (o1, o2, sizeof o1) == 0);
    dp_state_filter_kalman_destroy (a);
    dp_state_filter_kalman_destroy (b);
    dp_state_filter_kalman_destroy (NULL);
  }

  /* ── Blob, configure, cross-component rejects ────────────────────────────
   */
  {
    const double q = dp_state_filter_kalman_ratio_for_bandwidth (0.02);
    dp_state_filter_kalman_state_t *a
        = dp_state_filter_kalman_create_qr (q, 1.0, 1.0, 3.0);
    dp_state_filter_kalman_state_t *b
        = dp_state_filter_kalman_create_qr (q, 1.0, 1.0, 3.0);
    dp_state_filter_kalman_state_t *c
        = dp_state_filter_kalman_create_qr (q * 4, 2.0, 2.0, 1.0);
    DP_CHECK (a && b && c);
    uint32_t seed = 99;
    double   x[300], whole[300], tail[150];
    for (int i = 0; i < 300; i++)
      x[i] = 0.3 * ((double)dp_xs32 (&seed) / 4294967296.0 - 0.5);
    dp_state_filter_kalman_steps (a, x, whole, 300);
    /* A mid-stream split resumes bit for bit, mid-transient: the covariance
     * and the gains in force are in the blob. */
    dp_state_filter_kalman_steps (b, x, tail, 150);
    const size_t n    = dp_state_filter_kalman_state_bytes (b);
    uint8_t     *blob = malloc (n);
    dp_state_filter_kalman_get_state (b, blob);
    DP_CHECK (dp_state_filter_kalman_set_state (c, blob) == DP_OK);
    dp_state_filter_kalman_steps (c, x + 150, tail, 150);
    DP_CHECK (memcmp (tail, whole + 150, sizeof tail) == 0);
    /* Reset after a restore goes back to the SAME start (P0 is in the blob).
     */
    dp_state_filter_kalman_reset (c);
    DP_CHECK (memcmp (c->p, a->p0, sizeof c->p)
              == 0); /* the start, from the blob */

    /* Envelope and key rejects: every one leaves the target untouched. */
    dp_state_filter_kalman_state_t keep;
    uint8_t                       *bad = malloc (n);
    memcpy (bad, blob, n);
    bad[0] ^= 0xFF;
    keep = *c;
    DP_CHECK (dp_state_filter_kalman_set_state (c, bad) == DP_ERR_INVALID);
    DP_CHECK (memcmp (&keep, c, sizeof keep) == 0);
    /* offsets: hdr, then 7 base f64, p[6], p0[6], q, r (no flavor word: the
     * Kalman blob has its own magic) */
    const size_t hdr = sizeof (dp_state_hdr_t), base = hdr;
    const size_t o_p = base + 7 * 8, o_p0 = o_p + 6 * 8, o_q = o_p0 + 6 * 8;
    struct
    {
      size_t      off;
      double      v;
      const char *what;
    } cases[] = {
      { o_p + 0, -1.0, "P00 negative" },
      { o_p + 8, 1e30, "P01 beyond positive definite" },
      { o_p0 + 3 * 8, -1.0, "P0 not positive definite" },
      { o_p + 5 * 8, NAN, "P NaN" },
      { o_q, -1.0, "q negative" },
      { o_q + 8, 0.0, "r zero" },
      { base, 0.5, "kp not the covariance's" },
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++)
      {
        memcpy (bad, blob, n);
        memcpy (bad + cases[i].off, &cases[i].v, 8);
        keep = *c;
        if (dp_state_filter_kalman_set_state (c, bad) != DP_ERR_INVALID)
          {
            printf ("kalman blob accepted: %s\n", cases[i].what);
            DP_CHECK (0);
          }
        DP_CHECK (memcmp (&keep, c, sizeof keep) == 0);
      }
    /* A fixed-gain blob has the other magic: refused here, and the Kalman
     * blob is refused by the fixed-gain filter, both leaving the target. */
    {
      dp_state_filter_state_t *fx2 = dp_state_filter_create (0.02, 1.0);
      uint8_t                 *fb = malloc (dp_state_filter_state_bytes (fx2));
      dp_state_filter_get_state (fx2, fb);
      keep = *c;
      DP_CHECK (dp_state_filter_kalman_set_state (c, fb) == DP_ERR_INVALID);
      DP_CHECK (memcmp (&keep, c, sizeof keep) == 0);
      free (fb);
      dp_state_filter_destroy (fx2);
    }
    /* ... and the other way: a Kalman blob is not a fixed-gain filter's. */
    dp_state_filter_state_t *fx = dp_state_filter_create (0.02, 1.0);
    DP_CHECK (dp_state_filter_set_state (fx, blob) == DP_ERR_INVALID);
    dp_state_filter_destroy (fx);
    free (bad);
    free (blob);

    /* configure keeps lock, refuses a bad figure without changing anything. */
    const double integ = a->base.integ, rate = a->base.rate;
    DP_CHECK (
        dp_state_filter_kalman_configure (a, 1.0, sqrt (q * 4), 1.0, 0.0, 0.0)
        == DP_OK);
    DP_CHECK (a->base.integ == integ && a->base.rate == rate);
    keep = *a;
    /* a negative figure whose SQUARE is in range: refused for the sign */
    DP_CHECK (
        dp_state_filter_kalman_configure (a, 1.0, -sqrt (q * 4), 1.0, 0.0, 0.0)
        == DP_ERR_INVALID);
    DP_CHECK (memcmp (&keep, a, sizeof keep) == 0);
    /* the minimal pair: the same call with the sign made good is accepted, so
     * the refusal above is for the sign and for nothing else */
    DP_CHECK (
        dp_state_filter_kalman_configure (a, 1.0, sqrt (q * 4), 1.0, 0.0, 0.0)
        == DP_OK);
    dp_state_filter_kalman_destroy (a);
    dp_state_filter_kalman_destroy (b);
    dp_state_filter_kalman_destroy (c);
  }

  /* ── The Kalman flavor: the figures a caller gives ───────────────────────
   */
  {
    /* sigma_e and sigma_a are the square roots of r and q: the same filter. */
    const double q  = dp_state_filter_kalman_ratio_for_bandwidth (0.03);
    const double se = 0.7, sa = 0.7 * sqrt (q);
    dp_state_filter_kalman_state_t *a
        = dp_state_filter_kalman_create (se, sa, 2.0, 0.0);
    dp_state_filter_kalman_state_t *b
        = dp_state_filter_kalman_create_qr (sa * sa, se * se, 2.0, 1.0);
    DP_CHECK (a && b);
    DP_CHECK (memcmp (&a->base, &b->base, sizeof a->base) == 0);
    DP_CHECK (memcmp (a->p, b->p, sizeof a->p) == 0);
    /* The automatic case: bn and t alone is the same filter at unit r. */
    dp_state_filter_kalman_state_t *c
        = dp_state_filter_kalman_create_bn (0.015, 2.0);
    DP_CHECK (c && fabs (c->base.kr - b->base.kr) <= 1e-12 * b->base.kr);
    DP_CHECK (fabs (c->base.bn * c->base.t - 0.03) <= 1e-9);
    /* A frequency uncertainty is in the loop's units: its own steady-state
     * deviation is the lower edge, START_MAX times it the upper. */
    const double sf = sqrt (a->p0[3]);
    DP_CHECK (dp_state_filter_kalman_start_for (se, sa, sf * 0.99) == 0.0);
    DP_CHECK (fabs (dp_state_filter_kalman_start_for (se, sa, sf * 3.0) - 3.0)
              < 1e-12);
    DP_CHECK (dp_state_filter_kalman_start_for (
                  se, sa, sf * (STATE_FILTER_KALMAN_START_MAX + 0.1))
              == 0.0);
    DP_CHECK (dp_state_filter_kalman_create (se, sa, 2.0, sf * 0.5) == NULL);
    dp_state_filter_kalman_state_t *big
        = dp_state_filter_kalman_create (se, sa, 2.0, sf * 4.0);
    DP_CHECK (big && big->p0[3] > 15.9 * a->p0[3]);
    DP_CHECK (!dp_state_filter_kalman_params_ok (0.0, sa, 2.0, 0.0));
    /* each figure must be positive itself: -sa and -se have in-range squares
     */
    DP_CHECK (!dp_state_filter_kalman_params_ok (se, -sa, 2.0, 0.0));
    DP_CHECK (!dp_state_filter_kalman_params_ok (-se, sa, 2.0, 0.0));
    DP_CHECK (!dp_state_filter_kalman_params_ok (-se, -sa, 2.0, 0.0));
    DP_CHECK (dp_state_filter_kalman_create (se, -sa, 2.0, 0.0) == NULL);
    DP_CHECK (dp_state_filter_kalman_start_for (-se, sa, 0.0) == 0.0);
    /* the minimal pairs: each refusal above, with its sign made good, passes
     * (so each refusal is for the sign and for nothing else) */
    DP_CHECK (dp_state_filter_kalman_params_ok (se, sa, 2.0, 0.0));
    {
      dp_state_filter_kalman_state_t *good
          = dp_state_filter_kalman_create (se, sa, 2.0, 0.0);
      DP_CHECK (good != NULL);
      dp_state_filter_kalman_destroy (good);
    }
    DP_CHECK (dp_state_filter_kalman_start_for (se, sa, 0.0) == 1.0);
    /* the bandwidth range is inclusive at both ends and refuses just outside
     */
    DP_CHECK (dp_state_filter_kalman_params_ok_bn (STATE_FILTER_KALMAN_BNT_MIN,
                                                   1.0));
    DP_CHECK (!dp_state_filter_kalman_params_ok_bn (
        STATE_FILTER_KALMAN_BNT_MIN * (1.0 - 1e-6), 1.0));
    DP_CHECK (dp_state_filter_kalman_params_ok_bn (STATE_FILTER_KALMAN_BNT_MAX,
                                                   1.0));
    DP_CHECK (!dp_state_filter_kalman_params_ok_bn (
        STATE_FILTER_KALMAN_BNT_MAX * (1.0 + 1e-6), 1.0));
    DP_CHECK (!dp_state_filter_kalman_params_ok_bn (0.5, 1.0));
    DP_CHECK (!dp_state_filter_kalman_params_ok_bn (0.1, 2.0)); /* bn*t 0.2 */
    DP_CHECK (dp_state_filter_kalman_params_ok_bn (0.05, 2.0));
    DP_CHECK (dp_state_filter_kalman_create_bn (0.0, 1.0) == NULL);
    /* init outside the domain is defined: a zero covariance, zero gains. */
    dp_state_filter_kalman_state_t z;
    memset (&z, 0xA5, sizeof z);
    dp_state_filter_kalman_init_qr (&z, -1.0, 1.0, 1.0, 1.0);
    DP_CHECK (z.base.kp == 0.0 && z.base.ki == 0.0 && z.base.kr == 0.0);
    for (int i = 0; i < 6; i++)
      DP_CHECK (z.p[i] == 0.0 && z.p0[i] == 0.0);
    /* init is defined at every input: a figure that is not positive leaves
     * the inert filter (also for a NEGATIVE figure with an in-range square),
     * an unaccepted sigma_f0 is taken as the steady state, and a bandwidth
     * outside the flavor's range builds the live filter those figures
     * describe (params_ok is the check). */
    {
      dp_state_filter_kalman_state_t w;
      memset (&w, 0xA5, sizeof w);
      dp_state_filter_kalman_init (&w, se, -sa, 2.0, 0.0);
      DP_CHECK (w.base.kp == 0.0 && w.base.kr == 0.0 && w.p[0] == 0.0);
      memset (&w, 0xA5, sizeof w);
      dp_state_filter_kalman_init (&w, 0.0, sa, 2.0, 0.0);
      DP_CHECK (w.base.kp == 0.0 && w.p0[3] == 0.0);
      dp_state_filter_kalman_init (&w, se, sa, 2.0, sf * 0.5); /* refused */
      DP_CHECK (memcmp (w.p0, a->p0, sizeof w.p0) == 0);
      dp_state_filter_kalman_init (&w, 1.0, 1.0, 2.0, 0.0); /* bn*t too big */
      DP_CHECK (!dp_state_filter_kalman_params_ok (1.0, 1.0, 2.0, 0.0));
      DP_CHECK (isfinite (w.p[0]) && w.base.kp > 0.0 && w.base.kr > 0.0);
    }
    /* configure in the figures, keeping lock; a bad figure changes nothing. */
    a->base.integ = 0.25;
    DP_CHECK (dp_state_filter_kalman_configure (a, se, sa * 2.0, 2.0, 0.0, 0.0)
              == DP_OK);
    DP_CHECK (a->base.integ == 0.25);
    dp_state_filter_kalman_state_t keep = *a;
    DP_CHECK (dp_state_filter_kalman_configure (a, se, -sa, 2.0, 0.0, 0.0)
              == DP_ERR_INVALID);
    DP_CHECK (dp_state_filter_kalman_configure (a, -se, sa, 2.0, 0.0, 0.0)
              == DP_ERR_INVALID);
    DP_CHECK (memcmp (&keep, a, sizeof keep) == 0);
    /* the minimal pair for both refusals: the same call with the figures
     * positive is accepted */
    DP_CHECK (dp_state_filter_kalman_configure (a, se, sa, 2.0, 0.0, 0.0)
              == DP_OK);
    DP_CHECK (dp_state_filter_kalman_configure (a, 0.0, 0.0, 2.0, 0.0, 0.01)
              == DP_OK);
    DP_CHECK (dp_state_filter_kalman_configure (a, 0.0, 0.0, 2.0, 0.0, 9.0)
              == DP_ERR_INVALID);
    /* an argument of 0 keeps its current value: naming only sigma_a changes
     * only the rate-walk figure, and a call with every argument 0 restarts
     * the schedule with the figures as they are */
    DP_CHECK (dp_state_filter_kalman_configure (a, 1.0, 1e-3, 2.0, 0.0, 0.0)
              == DP_OK);
    const double r_before = a->r;
    DP_CHECK (dp_state_filter_kalman_configure (a, 0.0, 2e-3, 0.0, 0.0, 0.0)
              == DP_OK);
    DP_CHECK (a->r == r_before && fabs (a->q - 4e-6) < 1e-18
              && a->base.t == 2.0);
    a->base.integ = 0.5;
    dp_state_filter_kalman_step (a, 0.1);
    DP_CHECK (dp_state_filter_kalman_configure (a, 0.0, 0.0, 0.0, 0.0, 0.0)
              == DP_OK);
    DP_CHECK (memcmp (a->p, a->p0, sizeof a->p) == 0 && a->base.integ != 0.0);
    dp_state_filter_kalman_destroy (a);
    dp_state_filter_kalman_destroy (b);
    dp_state_filter_kalman_destroy (c);
    dp_state_filter_kalman_destroy (big);
  }

  /* ── ... the plain covariance update at the slow end ─────────────────────
   * Measured against a copy of the same recursion in long double by
   * native/validation/state_filter_kalman_loop.c (the report, section 2.7):
   * from the largest start, 10/(bn*t) updates, the gains stay within 3e-11
   * relative of the reference down to bn*t 1e-6, the worst entry is p12, whose
   * small value cancels (3e-5), and P is positive definite at every update; so
   * the plain form stays. This pins the slow end: from
   * the cap at bn*t 1e-5, P is positive definite at EVERY update over
   * 30/(bn*t) updates, finite, and the gains return to the doubling
   * solve's steady state. */
  {
    const double bnt = 1e-5;
    const double q   = dp_state_filter_kalman_ratio_for_bandwidth (bnt);
    /* the embedder's unguarded init: past the accepted start at this
     * bandwidth (the validated constructors refuse it), to pin the update */
    dp_state_filter_kalman_state_t *k
        = dp_state_filter_kalman_create_qr (q, 1.0, 1.0, 1.0);
    DP_REQUIRE (k != NULL);
    dp_state_filter_kalman_init_qr (k, q, 1.0, 1.0,
                                    STATE_FILTER_KALMAN_START_MAX);
    double ps[6], gs[3];
    DP_CHECK (dp_state_filter_kalman_steady (q, 1.0, ps, gs) == 1);
    int bad = 0;
    for (size_t n = 0; n < (size_t)(30.0 / bnt); n++)
      {
        dp_state_filter_kalman_step (k, 0.0);
        const double *p   = k->p;
        const double  l10 = p[1] / p[0], l20 = p[2] / p[0];
        const double  d1  = p[3] - l10 * p[1];
        const double  l21 = (p[4] - l20 * p[1]) / d1;
        const double  d2  = p[5] - l20 * p[2] - l21 * l21 * d1;
        if (!(p[0] > 0.0 && d1 > 0.0 && d2 > 0.0))
          bad++;
      }
    DP_CHECK (bad == 0);
    DP_CHECK (fabs (k->base.kr - gs[2]) <= 1e-6 * gs[2]);
    DP_CHECK (fabs (k->base.kp - gs[0]) <= 1e-6 * gs[0]);
    dp_state_filter_kalman_destroy (k);
  }

  /* ── sigma_f is the lower edge of the start ───────────────────────────── */
  {
    dp_state_filter_kalman_state_t *k
        = dp_state_filter_kalman_create (1.0, 1e-3, 1.0, 0.0);
    DP_REQUIRE (k != NULL);
    const double sf = dp_state_filter_kalman_sigma_f (k);
    DP_CHECK (fabs (sf - sqrt (k->p0[3])) <= 1e-12 * sf);
    DP_CHECK (dp_state_filter_kalman_params_ok (1.0, 1e-3, 1.0, sf * 1.001));
    DP_CHECK (!dp_state_filter_kalman_params_ok (1.0, 1e-3, 1.0, sf * 0.999));
    dp_state_filter_kalman_destroy (k);
    dp_state_filter_kalman_state_t z;
    memset (&z, 0, sizeof z);
    DP_CHECK (dp_state_filter_kalman_sigma_f (&z) == 0.0);
  }

  /* ── The shared harnesses: the blob round trip and chunk invariance ───────
   */
  {
    dp_state_filter_kalman_state_t *a
        = dp_state_filter_kalman_create (1.0, 2e-3, 1.0, 0.0);
    dp_state_filter_kalman_state_t *b
        = dp_state_filter_kalman_create (1.0, 2e-3, 1.0, 0.0);
    DP_REQUIRE (a && b);
    for (int i = 0; i < 25; i++)
      dp_state_filter_kalman_step (a, 0.2 - 0.01 * i);
    DP_STATE_ROUNDTRIP_TEST (dp_state_filter_kalman, a, b);
    dp_state_filter_kalman_destroy (a);
    dp_state_filter_kalman_destroy (b);

    enum
    {
      CN = 2000
    };
    static double x[CN];
    uint32_t      rng = 0xABCDu;
    for (int i = 0; i < CN; i++)
      x[i] = dp_uni (&rng) - 0.5;
    dp_ci_spec_t spec = {
      .name     = "state_filter_kalman steps",
      .create   = k_create_obj,
      .destroy  = k_destroy_obj,
      .process  = k_process,
      .in_size  = sizeof (double),
      .out_size = sizeof (double),
      .out_cap  = CN,
    };
    DP_CHECK (dp_chunk_invariance (&spec, x, CN) == 0);
  }

  /* ── The start rule: sampled (the constructor) against dense (here) ─────
   * The constructor checks every update up to 100 and then a 1% log-spaced
   * sample; the rule is every update. They must agree on accept
   * and refuse across the cap, at three bandwidths: the dense cap is found by
   * bisection and starts either side of it (and well inside and outside) are
   * compared: the sampled check is sound (it never accepts what the dense one
   * refuses) and agrees with it at and below 0.9 of the dense cap. */
  {
    const double bnts[3] = { 0.001, 0.06, 0.08 };
    for (int bi = 0; bi < 3; bi++)
      {
        const double q = dp_state_filter_kalman_ratio_for_bandwidth (bnts[bi]);
        double       lo = 1.0, hi = STATE_FILTER_KALMAN_START_MAX;
        if (dense_start_ok (q, hi))
          lo = hi; /* the ceiling is accepted: no cap below it here */
        else
          for (int it = 0; it < 14; it++)
            {
              const double m = 0.5 * (lo + hi);
              if (dense_start_ok (q, m))
                lo = m;
              else
                hi = m;
            }
        const double        cap = lo;
        static const double f[8]
            = { 0.5, 0.9, 0.98, 0.995, 1.005, 1.02, 1.1, 1.5 };
        for (int j = 0; j < 8; j++)
          {
            double st = cap * f[j];
            if (st < 1.0)
              st = 1.0;
            if (st > STATE_FILTER_KALMAN_START_MAX)
              continue;
            const int dense = dense_start_ok (q, st);
            const int smpl
                = dp_state_filter_kalman_params_ok_qr (q, 1.0, 1.0, st);
            /* the sampled check and the dense one agree on accept and
             * refuse, at, below and past the cap */
            if (dense != smpl)
              printf ("bn*t %g start %.4f: dense %d sampled %d\n", bnts[bi],
                      st, dense, smpl);
            DP_CHECK (dense == smpl);
          }
        /* the cap is the measured function of bn*t, not a constant: the
         * ceiling holds up to 0.04 (the rule's own cap is about 6 there), and
         * the rule falls past it (3.8 at 0.06, 2.4 at 0.08) */
        if (bi == 0)
          DP_CHECK (cap == STATE_FILTER_KALMAN_START_MAX);
        if (bi == 1)
          DP_CHECK (cap > 3.5 && cap < 4.0);
        if (bi == 2)
          DP_CHECK (cap > 2.2 && cap < 2.6);
      }
  }

  /* ── set_state accepts only a start a live object could hold ────────────
   * P0 must be the steady state with only the frequency entry scaled, by a
   * start from 1 to the ceiling. A start past the ceiling (the unguarded init
   * builds one) and a P0 off the steady state in any other entry are refused;
   * a start at the ceiling, and one between the rule's cap and the ceiling,
   * restore as they were. configure restarts the gain schedule and keeps the
   * estimates. */
  {
    const double q = dp_state_filter_kalman_ratio_for_bandwidth (0.08);
    dp_state_filter_kalman_state_t a, b;
    dp_state_filter_kalman_init_qr (&a, q, 1.0, 1.0,
                                    STATE_FILTER_KALMAN_START_MAX);
    dp_state_filter_kalman_init_qr (&b, q, 1.0, 1.0, 1.0);
    /* ceiling, past the rule's cap at 0.08 (2.4): restores */
    uint8_t *blob = malloc (dp_state_filter_kalman_state_bytes (&a));
    dp_state_filter_kalman_get_state (&a, blob);
    DP_CHECK (dp_state_filter_kalman_set_state (&b, blob) == DP_OK);
    DP_CHECK (memcmp (b.p0, a.p0, sizeof a.p0) == 0);
    /* past the ceiling: refused, and the target is untouched */
    dp_state_filter_kalman_state_t big, keep;
    dp_state_filter_kalman_init_qr (&big, q, 1.0, 1.0,
                                    STATE_FILTER_KALMAN_START_MAX * 1.2);
    dp_state_filter_kalman_get_state (&big, blob);
    keep = b;
    DP_CHECK (dp_state_filter_kalman_set_state (&b, blob) == DP_ERR_INVALID);
    DP_CHECK (memcmp (&keep, &b, sizeof keep) == 0);
    /* a P0 that is not the steady state in a non-frequency entry */
    dp_state_filter_kalman_state_t off = a;
    off.p0[0] *= 1.001;
    dp_state_filter_kalman_get_state (&off, blob);
    DP_CHECK (dp_state_filter_kalman_set_state (&b, blob) == DP_ERR_INVALID);
    /* a start below 1 is not a start */
    dp_state_filter_kalman_state_t low;
    dp_state_filter_kalman_init_qr (&low, q, 1.0, 1.0, 1.0);
    low.p0[3] *= 0.5; /* a frequency variance under the steady state's */
    dp_state_filter_kalman_get_state (&low, blob);
    DP_CHECK (dp_state_filter_kalman_set_state (&b, blob) == DP_ERR_INVALID);
    free (blob);

    /* configure: the estimates carry, the schedule restarts */
    dp_state_filter_kalman_state_t *c
        = dp_state_filter_kalman_create_qr (q, 1.0, 1.0, 1.5);
    DP_REQUIRE (c != NULL);
    for (int i = 0; i < 40; i++)
      dp_state_filter_kalman_step (c, 0.05);
    const double integ = c->base.integ, rate = c->base.rate;
    DP_CHECK (memcmp (c->p, c->p0, sizeof c->p) != 0); /* mid-transient */
    DP_CHECK (dp_state_filter_kalman_configure (
                  c, 1.0, sqrt (q * 1.3), 1.0,
                  1.5 * dp_state_filter_kalman_sigma_f (c), 0.0)
              == DP_OK);
    DP_CHECK (c->base.integ == integ && c->base.rate == rate);
    DP_CHECK (memcmp (c->p, c->p0, sizeof c->p) == 0); /* restarted */
    dp_state_filter_kalman_destroy (c);
  }

  /* ── configure: what is kept, replaced and restarted ───────────────────────
   * Three behaviors, each pinned (docs: the configure doc in the header). */
  {
    /* (1) a filter built from bn holds the figures that bandwidth implies
     * (sigma_e 1, sigma_a the root of the noise ratio); naming sigma_e alone
     * keeps that sigma_a. */
    dp_state_filter_kalman_state_t *k
        = dp_state_filter_kalman_create_bn (0.01, 1.0);
    DP_REQUIRE (k != NULL);
    const double q0 = k->q;
    DP_CHECK (k->r == 1.0);
    DP_CHECK (dp_state_filter_kalman_configure (k, 2.0, 0.0, 0.0, 0.0, 0.0)
              == DP_OK);
    /* q goes through a square root and back: kept to rounding */
    DP_CHECK (k->r == 4.0 && fabs (k->q / q0 - 1.0) < 1e-12
              && k->base.t == 1.0);
    dp_state_filter_kalman_destroy (k);

    /* (2) a call with bn replaces the stored figures by those it implies: the
     * figures-form call that follows keeps THEM, not the built-in ones. */
    k = dp_state_filter_kalman_create (2.0, 1e-3, 1.0, 0.0);
    DP_REQUIRE (k != NULL);
    DP_CHECK (k->r == 4.0);
    DP_CHECK (dp_state_filter_kalman_configure (k, 0.0, 0.0, 0.0, 0.0, 0.02)
              == DP_OK);
    DP_CHECK (k->r == 1.0
              && k->q == dp_state_filter_kalman_ratio_for_bandwidth (0.02));
    const double q_bn = k->q;
    DP_CHECK (dp_state_filter_kalman_configure (k, 0.0, 1e3, 0.0, 0.0, 0.0)
              == DP_ERR_INVALID); /* a rate-walk figure past the bound */
    DP_CHECK (k->r == 1.0 && k->q == q_bn); /* ... changed nothing */
    DP_CHECK (dp_state_filter_kalman_configure (k, 0.0, 0.0, 2.0, 0.0, 0.0)
              == DP_OK); /* t is a unit: bn*t is unchanged */
    DP_CHECK (k->r == 1.0 && fabs (k->q / q_bn - 1.0) < 1e-12);
    dp_state_filter_kalman_destroy (k);

    /* (3) with every argument 0 the gains go to the steady state at once and
     * the estimates carry (ending a high-gain start early); sigma_f0 alone
     * re-opens them with the same figures. */
    dp_state_filter_kalman_state_t *ref
        = dp_state_filter_kalman_create (1.0, 2e-6, 1.0, 0.0);
    DP_REQUIRE (ref != NULL);
    const double sf = dp_state_filter_kalman_sigma_f (ref);
    k               = dp_state_filter_kalman_create (1.0, 2e-6, 1.0, 3.0 * sf);
    DP_REQUIRE (k != NULL);
    for (int i = 0; i < 60; i++)
      dp_state_filter_kalman_step (k, 0.05);
    const double integ_k = k->base.integ;
    DP_CHECK (fabs (k->base.kr - ref->base.kr) > 1e-3 * ref->base.kr);
    DP_CHECK (dp_state_filter_kalman_configure (k, 0.0, 0.0, 0.0, 0.0, 0.0)
              == DP_OK);
    DP_CHECK (k->base.integ == integ_k);
    DP_CHECK (fabs (k->base.kr - ref->base.kr) <= 1e-12 * ref->base.kr);
    DP_CHECK (memcmp (k->p0, ref->p0, sizeof k->p0) == 0);
    DP_CHECK (
        dp_state_filter_kalman_configure (k, 0.0, 0.0, 0.0, 3.0 * sf, 0.0)
        == DP_OK);
    DP_CHECK (k->q == ref->q && k->r == ref->r);
    DP_CHECK (fabs (k->p0[3] / ref->p0[3] - 9.0) < 1e-9);
    dp_state_filter_kalman_destroy (k);
    dp_state_filter_kalman_destroy (ref);
  }

  /* ── The binding's dispatch: the figures, or bn alone, never both ────── */
  {
    DP_CHECK (dp_state_filter_kalman_create_args (0.0, 0.0, 1.0, 0.0, 0.0)
              == NULL); /* neither */
    DP_CHECK (dp_state_filter_kalman_create_args (1.0, 1e-3, 1.0, 0.0, 0.01)
              == NULL); /* both */
    dp_state_filter_kalman_state_t *a
        = dp_state_filter_kalman_create_args (0.0, 0.0, 1.0, 0.0, 0.01);
    dp_state_filter_kalman_state_t *b
        = dp_state_filter_kalman_create_bn (0.01, 1.0);
    DP_REQUIRE (a && b);
    DP_CHECK (memcmp (a->p, b->p, sizeof a->p) == 0);
    DP_CHECK (dp_state_filter_kalman_configure (a, 0.0, 0.0, 1.0, 0.0, 0.02)
              == DP_OK);
    /* bn mixed with the figures is refused */
    DP_CHECK (dp_state_filter_kalman_configure (a, 1.0, 1e-3, 1.0, 0.0, 0.02)
              == DP_ERR_INVALID);
    /* the forwarded read surface is the base's */
    DP_CHECK (dp_state_filter_kalman_stable (a, 0.0, 1.0)
              == dp_state_filter_stable (&a->base, 0.0, 1.0));
    DP_CHECK (dp_state_filter_kalman_bandwidth (a, 0.0, 1.0)
              == dp_state_filter_bandwidth (&a->base, 0.0, 1.0));
    dp_state_filter_kalman_destroy (a);
    dp_state_filter_kalman_destroy (b);
  }

  /* The refusal's sentence names the argument that failed, and is NULL
   * exactly when create_args builds: each refusal beside its minimal pair
   * (the same call with that one argument made good), then every
   * combination of awkward values against the constructor itself. */
  {
    static const struct
    {
      double      e, a, t, f0, bn;
      const char *names;
    } refuse[] = {
      { 0, 0, 1, 0, 0, "bn alone" },
      { 1, 1e-3, 1, 0, 0.01, "not both" },
      { 1, 1e-3, 0, 0, 0, "t must" },
      { 0, 0, NAN, 0, 0.01, "t must" },
      { 0, 0, 1, 0, -0.01, "bn must be finite and positive" },
      { 0, 0, 1, 0, INFINITY, "bn must be finite and positive" },
      { 0, 0, 1, 0, 0.5, "bn*t is outside" },
      { 1, -1e-3, 1, 0, 0, "must both be > 0" },
      { 0, 1e-3, 1, 0, 0, "must both be > 0" },
      { 1, 1e-3, 1, -1.0, 0, "sigma_f0 must be 0" },
      { 1, 1e-3, 1, 1e9, 0, "sigma_f0 must be 0" },
      { 1, 1e-24, 1, 0, 0, "below the smallest" },
      { 1, 1.0, 1, 0, 0, "above the largest" },
    };
    for (size_t i = 0; i < sizeof refuse / sizeof *refuse; i++)
      {
        const char *w = dp_state_filter_kalman_args_why (
            refuse[i].e, refuse[i].a, refuse[i].t, refuse[i].f0, refuse[i].bn);
        DP_CHECK (w != NULL && strstr (w, refuse[i].names) != NULL);
        const char *why = "unset";
        DP_CHECK (dp_state_filter_kalman_create_args_why (
                      refuse[i].e, refuse[i].a, refuse[i].t, refuse[i].f0,
                      refuse[i].bn, &why)
                  == NULL);
        DP_CHECK (why == w);
      }
    static const double good[][5]
        = { { 1.0, 1e-3, 1.0, 0.0, 0.0 }, { 0.0, 0.0, 1.0, 0.0, 0.01 } };
    for (size_t i = 0; i < sizeof good / sizeof *good; i++)
      {
        const char                     *why = "unset";
        dp_state_filter_kalman_state_t *k
            = dp_state_filter_kalman_create_args_why (good[i][0], good[i][1],
                                                      good[i][2], good[i][3],
                                                      good[i][4], &why);
        DP_CHECK (k != NULL && why == NULL);
        dp_state_filter_kalman_destroy (k);
      }
    DP_CHECK (dp_state_filter_kalman_create_args_why (0, 0, 1, 0, 0, NULL)
              == NULL);

    static const double es[]  = { -1.0, 0.0, 1.0, NAN };
    static const double as[]  = { -1e-3, 0.0, 1e-3, 1e-1, 1e-9, NAN };
    static const double ts[]  = { 0.0, 1.0, NAN };
    static const double fs[]  = { 0.0, -1.0, 1e9 };
    static const double bs[]  = { 0.0, 0.01, 0.5, -1.0 };
    int                 agree = 1;
    for (size_t ie = 0; ie < sizeof es / sizeof *es; ie++)
      for (size_t ia = 0; ia < sizeof as / sizeof *as; ia++)
        for (size_t it = 0; it < sizeof ts / sizeof *ts; it++)
          for (size_t jf = 0; jf < sizeof fs / sizeof *fs; jf++)
            for (size_t ib = 0; ib < sizeof bs / sizeof *bs; ib++)
              {
                const char *w = dp_state_filter_kalman_args_why (
                    es[ie], as[ia], ts[it], fs[jf], bs[ib]);
                dp_state_filter_kalman_state_t *k
                    = dp_state_filter_kalman_create_args (
                        es[ie], as[ia], ts[it], fs[jf], bs[ib]);
                if ((w == NULL) != (k != NULL))
                  agree = 0;
                dp_state_filter_kalman_destroy (k);
              }
    DP_CHECK (agree);
  }

  DP_TEST_END ("test_state_filter_kalman_core");
}
