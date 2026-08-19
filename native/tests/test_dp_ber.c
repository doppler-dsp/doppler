/**
 * @file test_dp_ber.c
 * @brief Tests for the error-rate harness itself (native/tests/dp_ber_test.h).
 *
 * The harness exists to stop a receiver test reporting a confidently wrong
 * number, so the harness is exactly the thing that must not be wrong. Each
 * block below pins one of its claims, and several are deliberately built to
 * fail if the historic footgun were reintroduced:
 *
 *   - the confidence interval is EXACT, checked against the closed form at
 *     `r = 1` and by Monte-Carlo coverage at `r = 20`;
 *   - the alignment is DETECTED, so a marker correlated against unrelated
 *     symbols must be REJECTED, not resolved to a plausible lag;
 *   - scoring does NOT search: handed a wrong lag it must report chance. A
 *     `min over (lag, rotation)` implementation would quietly rescue it, and
 *     that is the bug this whole file is insurance against;
 *   - the marker symbols are excluded from the scored window;
 *   - the sanity gate rejects "better than theory" and rejects a spinning
 *     constellation that both truth-free validators would otherwise pass.
 */
#include "dp_ber_test.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- stimulus helpers ---------------------------------------------------- */

/* rx[i] = constellation(truth[i + lag]) rotated by `phase`, plus AWGN at the
 * given per-quadrature sigma. Exactly the convention dp_ber_sync() resolves.
 */
static void
build (float complex *rx, size_t n_rx, const uint8_t *truth, size_t n_truth,
       int m, long lag, double phase, double sigma, uint64_t *st)
{
  double phi0 = mpsk_phi0 (m);
  for (size_t i = 0; i < n_rx; i++)
    {
      long   t  = (long)i + lag;
      double th = phase;
      if (t >= 0 && (size_t)t < n_truth)
        th += 2.0 * MPSK_PI * (double)truth[t] / (double)m + phi0;
      /* Sequenced. Two calls in one expression are INDETERMINATELY
         sequenced (C11 6.5.2.2p10) — not UB, but the order is the
         compiler's, and gcc and clang genuinely differ here: gcc evaluates
         the imaginary operand first, clang the real one. `make test` is gcc
         and `make coverage` is clang, so this line drew two different noise
         streams depending on the job. Pinned to gcc's order, the one the
         assertions were tuned against. */
      double n_im = sigma * dp_gauss64 (st);
      double n_re = sigma * dp_gauss64 (st);
      rx[i]       = (float)(cos (th) + n_re) + (float)(sin (th) + n_im) * I;
    }
}

/* --- 1. theory ----------------------------------------------------------- */

static void
test_theory (void)
{
  /* The textbook anchors. BPSK hits 1e-5 at 9.6 dB; QPSK's SER is ~2x BPSK's
     at the same Es/N0 because it carries two bits in the same energy. */
  DP_CHECK (fabs (dp_ber_theory_ser (2, pow (10.0, 0.96)) - 9.7e-6) < 1.0e-6);
  DP_CHECK_NEAR (dp_ber_theory_ser (4, 10.0), 2.0 * dp_ber_qfunc (sqrt (10.0)),
                 1e-15);
  DP_CHECK (dp_ber_theory_ser (8, 10.0) > dp_ber_theory_ser (4, 10.0));

  /* Gray QPSK is BPSK per bit: same BER at the same Eb/N0, i.e. QPSK at
     3 dB more Es/N0 than BPSK. */
  DP_CHECK_NEAR (dp_ber_theory_ber (4, 2.0 * 10.0),
                 dp_ber_theory_ber (2, 10.0), 1e-12);

  /* The SER=1e-3 operating points quoted in the loop-design rules. */
  DP_CHECK_NEAR (dp_ber_esn0_db_for_ser (2, 1e-3), 6.8, 0.15);
  DP_CHECK_NEAR (dp_ber_esn0_db_for_ser (4, 1e-3), 10.3, 0.15);
  DP_CHECK_NEAR (dp_ber_esn0_db_for_ser (8, 1e-3), 15.7, 0.15);

  /* Round-trip: the inverse really inverts. */
  for (int m = 2; m <= 8; m *= 2)
    for (double db = 4.0; db <= 18.0; db += 2.0)
      {
        double s = dp_ber_theory_ser (m, pow (10.0, db / 10.0));
        DP_CHECK_NEAR (dp_ber_esn0_db_for_ser (m, s), db, 0.01);
      }
}

/* --- 2. the confidence interval ------------------------------------------ */

static void
test_ci_exact (void)
{
  /* r = 1 has a closed form: the interval is [-ln(1-a/2)/N, -ln(a/2)/N].
     At 99% that is [0.0050125/N, 5.29832/N]. This is the case a normal
     approximation gets worst, and the one that proves the quantiles really
     come from the exact Gamma relation. */
  dp_ber_ci_t c = dp_ber_ci (1, 1000, 0.99);
  DP_CHECK_NEAR (c.lo * 1000.0, -log (0.995), 1e-6);
  DP_CHECK_NEAR (c.hi * 1000.0, -log (0.005), 1e-6);
  DP_CHECK_NEAR (c.rel, 1.0, 1e-12);

  /* r = 0 is not an error: the one-sided exact bound p <= -ln(alpha)/N still
     holds, and reporting it is how "no errors in N symbols" is stated. */
  c = dp_ber_ci (0, 10000, 0.99);
  DP_CHECK (c.p_hat == 0.0);
  DP_CHECK (c.lo == 0.0);
  DP_CHECK_NEAR (c.hi * 10000.0, -log (0.01), 1e-6);

  /* The point estimate is the UNBIASED (r-1)/(N-1), not the naive r/N. */
  c = dp_ber_ci (200, 200000, 0.99);
  DP_CHECK_NEAR (c.p_hat, 199.0 / 199999.0, 1e-15);
  DP_CHECK_NEAR (c.rel, 1.0 / sqrt (200.0), 1e-12);
  DP_CHECK (c.lo < c.p_hat && c.p_hat < c.hi);

  /* The width is set by the ERROR count alone -- the property that makes
     inverse binomial sampling worth the trouble. Two runs with the same r and
     wildly different N must have the same RELATIVE interval. (Not to the last
     bit: the limits scale as 1/N while the unbiased point estimate carries a
     1/(N-1), so the ratios agree to O(1/N), not exactly.) */
  {
    dp_ber_ci_t a = dp_ber_ci (200, 200000, 0.99);
    dp_ber_ci_t b = dp_ber_ci (200, 20000000, 0.99);
    DP_CHECK_NEAR (a.hi / a.p_hat, b.hi / b.p_hat, 1e-3);
    DP_CHECK_NEAR (a.lo / a.p_hat, b.lo / b.p_hat, 1e-3);
    DP_CHECK_NEAR (a.hi * 200000.0, b.hi * 20000000.0,
                   1e-9); /* exact in N*p */
  }

  /* More errors -> tighter, monotonically. */
  {
    double prev = INFINITY;
    for (unsigned long r = 2; r <= 512; r *= 2)
      {
        dp_ber_ci_t k = dp_ber_ci (r, r * 1000, 0.99);
        double      w = k.hi / k.lo;
        DP_CHECK (w < prev);
        prev = w;
      }
  }

  /* 200 errors is the documented ~7% relative / ~+-18% at 99%. */
  {
    dp_ber_ci_t k = dp_ber_ci (200, 200000, 0.99);
    DP_CHECK (k.hi / k.p_hat > 1.13 && k.hi / k.p_hat < 1.25);
    DP_CHECK (k.lo / k.p_hat > 0.79 && k.lo / k.p_hat < 0.89);
  }

  /* A wider confidence level must give a wider interval. */
  {
    dp_ber_ci_t a = dp_ber_ci (50, 50000, 0.95);
    dp_ber_ci_t b = dp_ber_ci (50, 50000, 0.99);
    DP_CHECK (b.lo < a.lo && b.hi > a.hi);
  }
}

/* Monte-Carlo the actual coverage: run true inverse binomial sampling at a
 * known rate and count how often the 99% interval contains it. This is the
 * only test that can catch a quantile that is subtly the wrong tail. */
static void
test_ci_coverage (void)
{
  const unsigned long r = 20;
  const double        p = 0.01, conf = 0.99;
  const int           trials  = 3000;
  uint64_t            st      = 0xC0FFEEu;
  int                 covered = 0;

  for (int t = 0; t < trials; t++)
    {
      unsigned long errs = 0, n = 0;
      dp_ber_ci_t   c;
      while (errs < r)
        {
          n++;
          if (dp_uni64 (&st) < p)
            errs++;
        }
      c = dp_ber_ci (errs, n, conf);
      if (c.lo <= p && p <= c.hi)
        covered++;
    }
  /* Exact intervals are conservative, so coverage sits at or a little above
     the nominal level; a one-tail slip would drop it far below. */
  DP_CHECK ((double)covered / trials >= 0.975);
  DP_CHECK ((double)covered / trials <= 1.0);
  printf ("  CI coverage at 99%%, r=%lu: %.3f\n", r, (double)covered / trials);
}

/* --- 3. alignment is detected, not searched ------------------------------ */

static void
test_sync_resolves (void)
{
  enum
  {
    NSYM = 6000
  };
  static uint8_t       truth[NSYM];
  static float complex rx[NSYM];
  uint64_t             st = 12345u;
  int                  m  = 4;

  for (int i = 0; i < NSYM; i++)
    truth[i] = (uint8_t)(dp_xs64 (&st) % 4u);

  /* Several (lag, phase) combinations, including the negative lags an RRC
     front end really produces and a phase far from any constellation point. */
  {
    long   lags[]   = { 0, 7, -34, 137, -180 };
    double phases[] = { 0.0, 0.31, -1.9, 2.7 };
    for (size_t li = 0; li < sizeof lags / sizeof *lags; li++)
      for (size_t pi = 0; pi < sizeof phases / sizeof *phases; pi++)
        {
          dp_ber_sync_t   sy;
          dp_ber_marker_t mk = { NULL, 256, 1000, 0, 0 };
          double          d;
          build (rx, NSYM, truth, NSYM, m, lags[li], phases[pi], 0.15, &st);
          sy = dp_ber_sync (rx, NSYM, truth, NSYM, &mk, m, DP_BER_LAG_SPAN,
                            DP_BER_SYNC_PFA);
          DP_CHECK (sy.ok);
          DP_CHECK (sy.lag == lags[li]);
          d = sy.phase - phases[pi];
          while (d > MPSK_PI)
            d -= 2.0 * MPSK_PI;
          while (d < -MPSK_PI)
            d += 2.0 * MPSK_PI;
          DP_CHECK_NEAR (d, 0.0, 0.05);
          DP_CHECK (sy.margin_db > 6.0);
          DP_CHECK (!sy.saturated);
        }
  }

  /* A lag outside the search must SATURATE and be reported as untrustworthy,
     not silently resolved to the nearest in-range peak. That saturation flag
     is what turned an "SER 0.48" mystery into a one-line diagnosis. */
  {
    dp_ber_sync_t   sy;
    dp_ber_marker_t mk = { NULL, 256, 1000, 0, 0 };
    build (rx, NSYM, truth, NSYM, m, 0, 0.0, 0.15, &st);
    sy = dp_ber_sync (rx, NSYM, truth, NSYM, &mk, m, 20, DP_BER_SYNC_PFA);
    DP_CHECK (sy.ok); /* lag 0 is inside +-20 */
    build (rx, NSYM, truth, NSYM, m, 137, 0.0, 0.15, &st);
    sy = dp_ber_sync (rx, NSYM, truth, NSYM, &mk, m, 20, DP_BER_SYNC_PFA);
    DP_CHECK (!sy.ok);
  }
}

/* The false-alarm gate: a marker correlated against a stream that does NOT
 * contain it must be rejected. A `min over lag` search cannot fail this test
 * because it never asks the question -- it always returns its best lag. */
static void
test_sync_rejects_garbage (void)
{
  enum
  {
    NSYM = 4000
  };
  static uint8_t       truth[NSYM], other[NSYM];
  static float complex rx[NSYM];
  uint64_t             st = 777u;
  int                  m = 4, alarms = 0;
  const int            trials = 50;

  for (int t = 0; t < trials; t++)
    {
      dp_ber_sync_t   sy;
      dp_ber_marker_t mk = { NULL, 256, 1000, 0, 0 };
      for (int i = 0; i < NSYM; i++)
        {
          truth[i] = (uint8_t)(dp_xs64 (&st) % 4u);
          other[i] = (uint8_t)(dp_xs64 (&st) % 4u);
        }
      /* rx carries `other`; the marker is taken from `truth`. */
      build (rx, NSYM, other, NSYM, m, 0, 0.0, 0.15, &st);
      sy = dp_ber_sync (rx, NSYM, truth, NSYM, &mk, m, DP_BER_LAG_SPAN,
                        DP_BER_SYNC_PFA);
      if (sy.ok)
        alarms++;
    }
  /* 50 trials x 401 lags at a 1e-6 whole-search Pfa: zero expected. */
  DP_CHECK (alarms == 0);
  printf ("  sync false alarms on unrelated data: %d/%d\n", alarms, trials);

  /* Pure noise, likewise. */
  {
    dp_ber_sync_t   sy;
    dp_ber_marker_t mk = { NULL, 256, 1000, 0, 0 };
    for (int i = 0; i < NSYM; i++)
      {
        truth[i]    = (uint8_t)(dp_xs64 (&st) % 4u);
        double n_im = dp_gauss64 (&st); /* gcc's order — see build() */
        double n_re = dp_gauss64 (&st);
        rx[i]       = (float)n_re + (float)n_im * I;
      }
    sy = dp_ber_sync (rx, NSYM, truth, NSYM, &mk, m, DP_BER_LAG_SPAN,
                      DP_BER_SYNC_PFA);
    DP_CHECK (!sy.ok);
  }

  /* A marker too short to identify an alignment says so, rather than
     returning a plausible wrong lag. sqrt(2*K*L) is the whole processing
     gain available, so 16 symbols genuinely cannot clear a 1e-6 gate over
     401 lags -- and the honest answer is "I cannot tell". */
  {
    dp_ber_sync_t   sy;
    dp_ber_marker_t mk = { NULL, 16, 1000, 0, 0 };
    for (int i = 0; i < NSYM; i++)
      truth[i] = (uint8_t)(dp_xs64 (&st) % 4u);
    build (rx, NSYM, truth, NSYM, m, 11, 0.0, 0.15, &st);
    sy = dp_ber_sync (rx, NSYM, truth, NSYM, &mk, m, DP_BER_LAG_SPAN,
                      DP_BER_SYNC_PFA);
    DP_CHECK (!sy.ok);
  }
}

/* --- 4. scoring does not search ------------------------------------------ */

static void
test_score_counts_exactly (void)
{
  enum
  {
    NSYM = 4000
  };
  static uint8_t       truth[NSYM];
  static float complex rx[NSYM];
  uint64_t             st = 24680u;
  int                  m  = 4;
  dp_ber_sync_t        sy;
  dp_ber_marker_t      mk = { NULL, 256, 500, 0, 0 };
  dp_ber_t             acc;

  for (int i = 0; i < NSYM; i++)
    truth[i] = (uint8_t)(dp_xs64 (&st) % 4u);
  build (rx, NSYM, truth, NSYM, m, 5, 0.4, 0.0, &st); /* noise-free */

  /* Inject exactly 37 symbol errors, each a single-step neighbour so the
     Gray mapping makes it exactly one bit error too. */
  for (int k = 0; k < 37; k++)
    {
      size_t i  = 1200 + (size_t)k * 13;
      long   t  = (long)i + 5;
      double th = 0.4 + mpsk_phi0 (m)
                  + 2.0 * MPSK_PI * (double)((truth[t] + 1) % m) / (double)m;
      rx[i]     = (float)cos (th) + (float)sin (th) * I;
    }

  sy = dp_ber_sync (rx, NSYM, truth, NSYM, &mk, m, DP_BER_LAG_SPAN,
                    DP_BER_SYNC_PFA);
  DP_CHECK (sy.ok);
  DP_CHECK (sy.lag == 5);

  dp_ber_init (&acc, m, 200);
  dp_ber_score (&acc, rx, 1000, NSYM, truth, NSYM, &mk, &sy);
  DP_CHECK (acc.errors == 37);
  DP_CHECK (acc.bit_errors == 37); /* Gray: one neighbour step == one bit */
  DP_CHECK (acc.bits == acc.symbols * 2);

  /* The marker symbols were EXCLUDED. The marker sits at truth [500, 756),
     i.e. rx [495, 751) -- entirely before the scored window here -- so widen
     the window and check the exclusion bites. */
  {
    dp_ber_t a2;
    dp_ber_init (&a2, m, 200);
    dp_ber_score (&a2, rx, 400, NSYM, truth, NSYM, &mk, &sy);
    DP_CHECK (a2.skipped >= 256);
    DP_CHECK (a2.symbols == (NSYM - 400) - a2.skipped);
  }

  /* THE anti-footgun assertion. Hand scoring a deliberately wrong lag and it
     must report chance (0.75 for QPSK), because it uses the alignment it was
     given and searches for nothing. If someone reintroduces a
     `min over (lag, rotation)` this drops to ~0 and the test fails. */
  {
    dp_ber_t      bad;
    dp_ber_sync_t wrong = sy;
    double        rate;
    wrong.lag = sy.lag + 17;
    dp_ber_init (&bad, m, 200);
    dp_ber_score (&bad, rx, 1000, NSYM, truth, NSYM, NULL, &wrong);
    rate = (double)bad.errors / (double)bad.symbols;
    DP_CHECK (rate > 0.6);
    printf ("  wrong-lag scoring reports %.3f (chance is 0.75)\n", rate);
  }

  /* Likewise a wrong ROTATION: no rotation search either. */
  {
    dp_ber_t      bad;
    dp_ber_sync_t wrong = sy;
    double        rate;
    wrong.phase = sy.phase + MPSK_PI / 2.0;
    dp_ber_init (&bad, m, 200);
    dp_ber_score (&bad, rx, 1000, NSYM, truth, NSYM, NULL, &wrong);
    rate = (double)bad.errors / (double)bad.symbols;
    DP_CHECK (rate > 0.9);
  }
}

/* --- 5. the settled window ----------------------------------------------- */

static void
test_settle (void)
{
  enum
  {
    N = 3000
  };
  static unsigned char f[N];

  /* The analytic budget: 2*(5/0.01 + 5/0.01) = 2000, never 500. */
  DP_CHECK (dp_test_settle_syms (0.01, 0.01) == 2000);
  DP_CHECK (dp_ber_settle (0.01, 0.01, NULL, NULL, 0, NULL) == 2000);

  /* A flag that goes high at 800 and stays high: lock at 800. */
  memset (f, 0, sizeof f);
  for (int i = 800; i < N; i++)
    f[i] = 1;
  DP_CHECK (dp_ber_lock_symbol (f, N, 200, 0.9) == 800);

  /* One late dip must NOT move the reported lock -- the failure that once
     reported 2286 instead of 415 and left no measurement window. */
  f[2500] = 0;
  DP_CHECK (dp_ber_lock_symbol (f, N, 200, 0.9) == 800);

  /* A detector that declares early then flaps fails the fraction test. */
  memset (f, 0, sizeof f);
  for (int i = 100; i < 400; i++)
    f[i] = 1;
  for (int i = 400; i < N; i++)
    f[i] = (i % 3) == 0;
  DP_CHECK (dp_ber_lock_symbol (f, N, 200, 0.9) < 0);

  /* Never locked -> -1, and dp_ber_settle reports ok = 0: there is no valid
     steady-state window and the caller must say so. */
  memset (f, 0, sizeof f);
  DP_CHECK (dp_ber_lock_symbol (f, N, 200, 0.9) == -1);
  {
    int ok = 1;
    dp_ber_settle (0.01, 0.01, f, NULL, N, &ok);
    DP_CHECK (!ok);
  }

  /* A lock LATER than the budget wins. */
  {
    static unsigned char t[N];
    int                  ok = 0;
    memset (t, 0, sizeof t);
    for (int i = 2400; i < N; i++)
      t[i] = 1;
    DP_CHECK (dp_ber_settle (0.01, 0.01, t, NULL, N, &ok) == 2400);
    DP_CHECK (ok);
  }

  /* The LATEST indicator decides, and the budget is a floor under all of
     them. The handover term this case used to check is gone with the
     handover itself (doppler#877); what remains is that two indicators are
     combined by max, which is checked with them DIFFERING so an
     implementation that read only one would fail. */
  {
    static unsigned char t[N], c[N];
    size_t               s;
    int                  ok = 0;
    memset (t, 0, sizeof t);
    memset (c, 0, sizeof c);
    for (int i = 500; i < N; i++)
      t[i] = 1;
    for (int i = 2600; i < N; i++)
      c[i] = 1;
    s = dp_ber_settle (0.01, 0.01, t, c, N, &ok);
    DP_CHECK (ok);
    /* 2600 > the 2000-symbol budget and > the timing lock at 500. */
    DP_CHECK (s == 2600);
    DP_CHECK (s > dp_test_settle_syms (0.01, 0.01));
  }
}

/* --- 6. the sanity gate -------------------------------------------------- */

static void
test_sanity_gate (void)
{
  enum
  {
    NSYM = 20000
  };
  static uint8_t       truth[NSYM];
  static float complex rx[NSYM];
  uint64_t             st      = 99991u;
  int                  m       = 4;
  double               esn0_db = 10.3; /* QPSK's SER = 1e-3 anchor */
  double               sigma   = sqrt (0.5 / pow (10.0, esn0_db / 10.0));
  dp_ber_t             acc;
  dp_ber_report_t      r;

  for (int i = 0; i < NSYM; i++)
    truth[i] = (uint8_t)(dp_xs64 (&st) % 4u);
  build (rx, NSYM, truth, NSYM, m, 9, 0.7, sigma, &st);

  /* The honest path: a clean measurement should PASS every gate and land
     within a fraction of a dB of the bound (this stimulus is the bound --
     there is no receiver in the loop, so the loss must be ~0). */
  dp_ber_init (&acc, m, 200);
  r = dp_ber_measure (&acc, rx, NSYM, truth, NSYM, esn0_db, 0, 1, NULL);
  dp_ber_print ("ideal QPSK @10.3dB", &r);
  DP_CHECK (r.aligned);
  DP_CHECK (r.sane);
  DP_CHECK (r.ser.lo <= dp_ber_theory_ser (m, pow (10.0, esn0_db / 10.0)));
  DP_CHECK (fabs (r.loss_db) < 1.0);
  DP_CHECK (fabs (r.evm_db + esn0_db) < 1.5);
  DP_CHECK (fabs (r.m2m4_db - esn0_db) < 1.5);

  /* "Better than theory" must be rejected. Forge an impossibly good count at
     the same Es/N0: this is what an alignment optimised over the answer, or
     scoring known symbols, actually looks like in the numbers. */
  {
    dp_ber_t        fake = acc;
    dp_ber_report_t f;
    dp_ber_sync_t   sy = { 0, 0.0, 100.0, 1.0, 40.0, 20.0, 1, 0, 0, 1, "ok" };
    fake.errors        = 2;
    fake.symbols       = 200000;
    f                  = dp_ber_report (&fake, esn0_db, &sy, 0, NSYM, 1, 0.99);
    DP_CHECK (!f.sane);
    DP_CHECK (!f.ok);
    printf ("  too-good-to-be-true rejected: %s\n", f.why);
  }

  /* The EVM floor is M-dependent, and knowing it is what makes the spin test
     below meaningful: a QPSK constellation with NO carrier recovery at all
     still reads -7.0 dB, not 0 dB. */
  DP_CHECK_NEAR (dp_test_evm_scatter_floor_db (2), -1.39, 0.02);
  DP_CHECK_NEAR (dp_test_evm_scatter_floor_db (4), -7.00, 0.02);
  DP_CHECK_NEAR (dp_test_evm_scatter_floor_db (8), -12.92, 0.02);

  /* A SPINNING constellation. Both truth-free validators are individually
     fooled -- M2M4 is rotation-blind and still reads a healthy SNR, the
     self-referenced EVM collapses to its scatter floor -- which is exactly
     how the real diagnosis was made (ber=0.404 evm=-2.5 dB m2m4=15.3 dB ->
     unrecovered carrier phase). Note it reads -6.3 dB, NOT ~0 dB: at QPSK
     that is only 0.7 dB above the floor, so the gate must compare against the
     floor rather than against zero. */
  {
    dp_ber_t        spin;
    dp_ber_report_t f;
    dp_ber_sync_t   sy = { 9, 0.7, 100.0, 1.0, 40.0, 20.0, 1, 0, 0, 1, "ok" };
    for (int i = 0; i < NSYM; i++)
      {
        double a  = 0.01 * (double)i;
        double cr = cos (a), sr = sin (a);
        double re = (double)crealf (rx[i]), im = (double)cimagf (rx[i]);
        rx[i] = (float)(re * cr - im * sr) + (float)(re * sr + im * cr) * I;
      }
    dp_ber_init (&spin, m, 200);
    dp_ber_score (&spin, rx, 1000, NSYM, truth, NSYM, NULL, &sy);
    f = dp_ber_report (&spin, esn0_db, &sy, 1000, NSYM, 1, 0.99);
    DP_CHECK (!f.sane);
    printf ("  spinning constellation rejected: %s  (evm %.1f m2m4 %.1f)\n",
            f.why, f.evm_db, f.m2m4_db);
  }

  /* An unsettled window is rejected on its own gate, because NEITHER
     truth-free validator can see one. Both truth-free validators PASS here --
     this exact stimulus was called sane a few lines above -- which is the
     whole point: a rotation-blind metric cannot see an unsettled window, so
     it will happily "confirm" a wrong one. Gate 1 is not redundant with
     gate 3. */
  {
    dp_ber_sync_t   sy = { 9, 0.7, 100.0, 1.0, 40.0, 20.0, 1, 0, 0, 1, "ok" };
    dp_ber_report_t f  = dp_ber_report (&acc, esn0_db, &sy, 0, NSYM, 0, 0.99);
    DP_CHECK (!f.ok);
    DP_CHECK (!f.settled);
    DP_CHECK (!f.sane); /* gate 1 fails first and shadows the rest */
  }

  /* dp_ber_measure() must PROPAGATE the caller's settled flag rather than
     assume it. Same stimulus, same everything, settled = 0 -> BAD. */
  {
    dp_ber_t        a3;
    dp_ber_report_t f;
    dp_ber_init (&a3, m, 200);
    f = dp_ber_measure (&a3, rx, NSYM, truth, NSYM, esn0_db, 0, 0, NULL);
    DP_CHECK (!f.settled);
    DP_CHECK (!f.ok);
  }
}

/* --- 7. the accumulate-until-enough loop --------------------------------- */

static void
test_inverse_sampling_loop (void)
{
  enum
  {
    NSYM = 4000
  };
  static uint8_t       truth[NSYM];
  static float complex rx[NSYM];
  uint64_t             st      = 5150u;
  int                  m       = 2;
  double               esn0_db = 6.8; /* BPSK's SER = 1e-3 anchor */
  double               sigma   = sqrt (0.5 / pow (10.0, esn0_db / 10.0));
  double          theory = dp_ber_theory_ser (m, pow (10.0, esn0_db / 10.0));
  dp_ber_t        acc;
  dp_ber_report_t r;
  int             bursts = 0;

  dp_ber_init (&acc, m, 60); /* 60 errors: ~13% relative, fast */
  while (!dp_ber_enough (&acc) && bursts < 400)
    {
      for (int i = 0; i < NSYM; i++)
        truth[i] = (uint8_t)(dp_xs64 (&st) & 1u);
      build (rx, NSYM, truth, NSYM, m, 3, -0.9, sigma, &st);
      dp_ber_measure (&acc, rx, NSYM, truth, NSYM, esn0_db, 0, 1, NULL);
      bursts++;
    }
  DP_CHECK (dp_ber_enough (&acc));
  r = dp_ber_report (&acc, esn0_db, NULL, 0, NSYM, 1, 0.99);
  printf ("  inverse sampling: %d bursts, r=%lu N=%lu, SER %.3e "
          "[%.3e, %.3e] vs theory %.3e\n",
          bursts, acc.errors, acc.symbols, r.ser.p_hat, r.ser.lo, r.ser.hi,
          theory);
  /* The stimulus IS the bound, so the interval must contain theory. This is
     the end-to-end proof that alignment, exclusion, counting and the interval
     are all mutually consistent. */
  DP_CHECK (r.ser.lo <= theory && theory <= r.ser.hi);
  DP_CHECK (r.ber.lo <= theory && theory <= r.ber.hi); /* BPSK: BER == SER */
}

int
main (void)
{
  printf ("test_dp_ber\n");
  test_theory ();
  test_ci_exact ();
  test_ci_coverage ();
  test_sync_resolves ();
  test_sync_rejects_garbage ();
  test_score_counts_exactly ();
  test_settle ();
  test_sanity_gate ();
  test_inverse_sampling_loop ();
  DP_TEST_END ("test_dp_ber");
}
