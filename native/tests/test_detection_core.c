#include "detection/detection_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>

#define CLOSE(a, b, tol) (fabs ((a) - (b)) < (tol))

int
main (void)
{

  /* ── marcum_q: special cases ─────────────────────────────────────── */

  /* b <= 0 must return 1.0 regardless of a and m. */
  DP_CHECK (marcum_q (1, 0.0, 0.0) == 1.0);
  DP_CHECK (marcum_q (1, 3.0, 0.0) == 1.0);
  DP_CHECK (marcum_q (3, 2.0, -1.0) == 1.0);

  /* ── marcum_q: Q_M(0, b) = exp(-v) * sum_{j=0}^{M-1} v^j/j! ──── */

  /* M=1: Q_1(0, b) = exp(-b^2/2) */
  DP_CHECK (CLOSE (marcum_q (1, 0.0, 1.0), exp (-0.5), 1e-12));
  DP_CHECK (CLOSE (marcum_q (1, 0.0, 2.0), exp (-2.0), 1e-12));

  /* M=2: Q_2(0, b) = exp(-v)*(1 + v),  v = b^2/2 */
  /* v = 2: Q_2(0, 2) = exp(-2)*(1+2) = 3*exp(-2) */
  DP_CHECK (CLOSE (marcum_q (2, 0.0, 2.0), 3.0 * exp (-2.0), 1e-12));

  /* ── marcum_q: values with nonzero a (reference: Python series) ── */
  DP_CHECK (CLOSE (marcum_q (1, 2.0, 1.0), 0.9181076963694063, 1e-10));
  DP_CHECK (CLOSE (marcum_q (1, 1.0, 2.0), 0.2690120600359100, 1e-10));
  DP_CHECK (CLOSE (marcum_q (2, 1.0, 1.0), 0.9407902191465286, 1e-10));
  DP_CHECK (CLOSE (marcum_q (1, 3.0, 2.0), 0.8867207544023923, 1e-10));

  /* ── marcum_q: monotonicity ──────────────────────────────────────── */
  /* Larger a  =>  higher probability (better SNR helps detection). */
  DP_CHECK (marcum_q (1, 1.0, 2.0) < marcum_q (1, 2.0, 2.0));
  /* Larger b  =>  lower probability (harder threshold to cross). */
  DP_CHECK (marcum_q (1, 2.0, 3.0) < marcum_q (1, 2.0, 1.0));
  /* Higher order M (more integration) with same a, b. */
  DP_CHECK (marcum_q (1, 1.5, 1.5) < marcum_q (2, 1.5, 1.5));

  /* ── det_threshold ───────────────────────────────────────────────── */
  /* Roundtrip: exp(-eta^2/2) must recover pfa. */
  {
    double eta4  = det_threshold (1e-4);
    double eta6  = det_threshold (1e-6);
    double eta10 = det_threshold (1e-10);
    DP_CHECK (CLOSE (exp (-0.5 * eta4 * eta4), 1e-4, 1e-14));
    DP_CHECK (CLOSE (exp (-0.5 * eta6 * eta6), 1e-6, 1e-14));
    DP_CHECK (CLOSE (exp (-0.5 * eta10 * eta10), 1e-10, 1e-14));
    /* Known value. */
    DP_CHECK (CLOSE (eta6, 5.256521769756932, 1e-10));
  }

  /* ── det_pd ──────────────────────────────────────────────────────── */
  {
    double eta = det_threshold (1e-6);

    /* At snr=0, Pd must equal Pfa (noise-only regime). */
    DP_CHECK (CLOSE (det_pd (0.0, 1, eta), 1e-6, 1e-14));
    DP_CHECK (CLOSE (det_pd (0.0, 4, eta), 1e-6, 1e-14));

    /* Higher SNR improves Pd. */
    DP_CHECK (det_pd (0.5, 4, eta) < det_pd (1.0, 4, eta));

    /* More dwell improves Pd at fixed SNR. */
    DP_CHECK (det_pd (0.5, 1, eta) < det_pd (0.5, 8, eta));

    /* Pd is bounded in [0, 1]. */
    DP_CHECK (det_pd (10.0, 64, eta) <= 1.0);
    DP_CHECK (det_pd (0.0, 1, eta) >= 0.0);
  }

  /* ── det_dwell ───────────────────────────────────────────────────── */
  {
    /* Very high SNR: single dwell is enough. */
    DP_CHECK (det_dwell (100.0, 0.9, 1e-6, 256) == 1);

    /* Extremely low SNR: cannot meet target within max_dwell. */
    DP_CHECK (det_dwell (0.001, 0.9, 1e-6, 10) == -1);

    /* Returned dwell achieves the target. */
    int m = det_dwell (0.5, 0.9, 1e-6, 512);
    DP_CHECK (m > 0);
    if (m > 0)
      {
        double eta = det_threshold (1e-6);
        DP_CHECK (det_pd (0.5, m, eta) >= 0.9);
        /* Previous dwell should not suffice (minimum dwell property). */
        if (m > 1)
          DP_CHECK (det_pd (0.5, m - 1, eta) < 0.9);
      }
  }

  /* ── det_snr ─────────────────────────────────────────────────────── */
  {
    /* Roundtrip: det_pd at returned SNR must meet pd_min. */
    double snr4  = det_snr (4, 0.9, 1e-6);
    double snr8  = det_snr (8, 0.9, 1e-6);
    double snr16 = det_snr (16, 0.9, 1e-6);
    double eta   = det_threshold (1e-6);

    DP_CHECK (det_pd (snr4, 4, eta) >= 0.9 - 1e-12);
    DP_CHECK (det_pd (snr8, 8, eta) >= 0.9 - 1e-12);
    DP_CHECK (det_pd (snr16, 16, eta) >= 0.9 - 1e-12);

    /* More dwell requires less SNR (coherent gain). */
    DP_CHECK (snr16 < snr8);
    DP_CHECK (snr8 < snr4);

    /* Result is non-negative. */
    DP_CHECK (snr4 >= 0.0);
  }

  /* ── det_threshold_power ─────────────────────────────────────────── */
  {
    /* Roundtrip: exp(-p) must recover pfa. */
    double p4  = det_threshold_power (1e-4);
    double p6  = det_threshold_power (1e-6);
    double p10 = det_threshold_power (1e-10);
    DP_CHECK (CLOSE (exp (-p4), 1e-4, 1e-14));
    DP_CHECK (CLOSE (exp (-p6), 1e-6, 1e-14));
    DP_CHECK (CLOSE (exp (-p10), 1e-10, 1e-14));
    /* Known value: -ln(1e-6) = 6·ln(10). */
    DP_CHECK (CLOSE (p6, 6.0 * log (10.0), 1e-12));
    /* Relationship to amplitude threshold: p = eta^2/2. */
    double eta6 = det_threshold (1e-6);
    DP_CHECK (CLOSE (p6, 0.5 * eta6 * eta6, 1e-12));
  }

  /* ── det_pd_power ────────────────────────────────────────────────── */
  {
    double p   = det_threshold_power (1e-6);
    double eta = det_threshold (1e-6);

    /* At snr_power=0, Pd must equal Pfa. */
    DP_CHECK (CLOSE (det_pd_power (0.0, 1, p), 1e-6, 1e-14));

    /* Equivalence with amplitude detector:
     * det_pd_power(snr^2, M, p) == det_pd(snr, M, eta)
     * because Q_1(sqrt(2M)*snr, eta) == Q_1(sqrt(2M*snr^2), sqrt(2p))
     * and eta = sqrt(2p). */
    DP_CHECK (CLOSE (det_pd_power (4.0, 1, p), det_pd (2.0, 1, eta), 1e-12));
    DP_CHECK (CLOSE (det_pd_power (1.0, 4, p), det_pd (1.0, 4, eta), 1e-12));

    /* Higher snr_power improves Pd. */
    DP_CHECK (det_pd_power (0.5, 4, p) < det_pd_power (2.0, 4, p));
    /* More dwell improves Pd at fixed snr_power. */
    DP_CHECK (det_pd_power (1.0, 1, p) < det_pd_power (1.0, 8, p));
  }

  /* ── det_dwell_power ─────────────────────────────────────────────── */
  {
    /* Equivalence: det_dwell_power(snr^2) == det_dwell(snr). */
    int m_amp = det_dwell (0.5, 0.9, 1e-6, 512);
    int m_pow = det_dwell_power (0.25, 0.9, 1e-6, 512); /* 0.25 = 0.5^2 */
    DP_CHECK (m_amp == m_pow);
    DP_CHECK (m_pow > 0);

    /* Returned dwell achieves the target for power detector. */
    if (m_pow > 0)
      {
        double p = det_threshold_power (1e-6);
        DP_CHECK (det_pd_power (0.25, m_pow, p) >= 0.9);
        if (m_pow > 1)
          DP_CHECK (det_pd_power (0.25, m_pow - 1, p) < 0.9);
      }
  }

  /* ── det_snr_power ───────────────────────────────────────────────── */
  {
    /* Roundtrip: det_pd_power at returned snr_power must meet pd_min. */
    double sp4 = det_snr_power (4, 0.9, 1e-6);
    double sp8 = det_snr_power (8, 0.9, 1e-6);
    double p   = det_threshold_power (1e-6);

    DP_CHECK (det_pd_power (sp4, 4, p) >= 0.9 - 1e-12);
    DP_CHECK (det_pd_power (sp8, 8, p) >= 0.9 - 1e-12);

    /* More dwell requires less power SNR. */
    DP_CHECK (sp8 < sp4);

    /* Equivalence: det_snr_power = det_snr^2. */
    double sa4 = det_snr (4, 0.9, 1e-6);
    DP_CHECK (CLOSE (sp4, sa4 * sa4, 1e-8));
  }

  /* ── det_ema_alpha ───────────────────────────────────────────────── */
  {
    /* No gain requested (or possible): no averaging. */
    DP_CHECK (det_ema_alpha (0.0, 0.0) == 1.0);
    DP_CHECK (det_ema_alpha (10.0, 5.0) == 1.0);

    /* alpha = 2*gin/(gin+gout): 20 dB gain -> 1/alpha = 50.5 regardless
       of where the pair sits on the dB axis (only the gain matters). */
    double a20 = det_ema_alpha (0.0, 20.0);
    DP_CHECK (CLOSE (1.0 / a20, 50.5, 1e-9));
    DP_CHECK (CLOSE (det_ema_alpha (10.0, 30.0), a20, 1e-12));

    /* The forward relation holds: SNR_out = SNR_in * (2 - a) / a. */
    double a    = det_ema_alpha (3.0, 27.0);
    double gain = (2.0 - a) / a;
    DP_CHECK (CLOSE (10.0 * log10 (gain), 24.0, 1e-9));
  }

  /* ── det_verify_count / det_verify_delay ─────────────────────────── */
  {
    /* Compounding: p^n <= target at the returned n, not at n-1. */
    DP_CHECK (det_verify_count (1e-3, 1e-6) == 2);
    DP_CHECK (det_verify_count (1e-3, 1e-9) == 3);
    DP_CHECK (det_verify_count (0.5, 1e-3) == 10);
    DP_CHECK (pow (0.5, 10) <= 1e-3 && pow (0.5, 9) > 1e-3);

    /* Exact log multiple stays exact (the pre-ceil nudge): 0.1^6 = 1e-6. */
    DP_CHECK (det_verify_count (0.1, 1e-6) == 6);

    /* Degenerate edges. */
    DP_CHECK (det_verify_count (1e-3, 0.5) == 1); /* budget already met  */
    DP_CHECK (det_verify_count (0.0, 1e-6) == 1); /* impossible look     */
    DP_CHECK (det_verify_count (1.0, 0.5) == INT_MAX); /* unreachable     */

    /* Run waiting time: classic 2-straight-heads = 6 tosses; the p -> 1
     * limit is exactly n; p = 0 never completes. */
    DP_CHECK (CLOSE (det_verify_delay (0.5, 2), 6.0, 1e-12));
    DP_CHECK (det_verify_delay (1.0, 8) == 8.0);
    DP_CHECK (isinf (det_verify_delay (0.0, 3)));
    DP_CHECK (CLOSE (det_verify_delay (0.9, 8),
                     (1.0 - pow (0.9, 8)) / (pow (0.9, 8) * 0.1), 1e-9));

    /* n clamps to 1: a run of one success is a plain geometric wait. */
    DP_CHECK (CLOSE (det_verify_delay (0.5, 0), 2.0, 1e-12));
  }

  /* ── det_threshold_f ─────────────────────────────────────────────── */
  {
    /* Exact special cases: F(2,2) tail is 1/(1+g), so the quantile is
     * (1-pfa)/pfa; F(4,4) reduces to the cubic I_x(2,2) = x^2(3-2x). */
    DP_CHECK (CLOSE (det_threshold_f (1e-3, 2), 999.0, 1e-6));
    DP_CHECK (CLOSE (det_threshold_f (1e-3, 4), 53.4358291, 1e-4));
    DP_CHECK (CLOSE (det_threshold_f (1e-3, 16), 5.2048, 5e-4));
    DP_CHECK (CLOSE (det_threshold_f (1e-3, 64), 2.1931, 5e-4));

    /* Monotone: more DOF hardens the estimate (smaller quantile); a
     * looser pfa lowers the gate. */
    DP_CHECK (det_threshold_f (1e-3, 8) > det_threshold_f (1e-3, 9));
    DP_CHECK (det_threshold_f (1e-2, 16) < det_threshold_f (1e-3, 16));

    /* Odd n is first-class (no even-n restriction). */
    double g15 = det_threshold_f (1e-3, 15), g16 = det_threshold_f (1e-3, 16);
    DP_CHECK (g15 > g16 && g16 > 1.0);

    /* Invalid inputs fail closed. */
    DP_CHECK (det_threshold_f (0.0, 16) == 0.0);
    DP_CHECK (det_threshold_f (1.0, 16) == 0.0);
    DP_CHECK (det_threshold_f (1e-3, 0) == 0.0);
  }

  /* ── det_q_inv ───────────────────────────────────────────────────── */
  {
    /* Inverts the tail it claims to: Q(eta) must come back to p. A stub
     * returning any constant fails this at every p. */
    const double ps[] = { 5e-6, 1e-5, 1e-3, 1e-2, 0.1, 0.3 };
    for (size_t i = 0; i < sizeof (ps) / sizeof (ps[0]); i++)
      {
        double eta = det_q_inv (ps[i]);
        DP_CHECK (eta > 0.0);
        DP_CHECK (
            CLOSE (0.5 * erfc (eta * M_SQRT1_2), ps[i], 1e-12 + 1e-9 * ps[i]));
      }

    /* The anchor the carrier lock metric is stated in: 0.5 / 0.1132 is
     * 4.42 sigma, a per-look Pfa of 5e-6 (carrier_nda_core.h). */
    DP_CHECK (CLOSE (det_q_inv (5e-6), 4.4172, 1e-4));

    /* It is NOT det_threshold, and that is why it exists: the envelope
     * law gives 4.9409 where this gives 4.4172. A check for "about 4-5"
     * would pass on the wrong function, so pin the GAP. */
    DP_CHECK (CLOSE (det_threshold (5e-6), 4.9409, 1e-4));
    DP_CHECK (det_threshold (5e-6) - det_q_inv (5e-6) > 0.5);

    /* Monotone decreasing, and fails closed at or past the median. */
    DP_CHECK (det_q_inv (1e-6) > det_q_inv (1e-3));
    DP_CHECK (det_q_inv (0.5) == 0.0);
    DP_CHECK (det_q_inv (0.0) == 0.0);
    DP_CHECK (det_q_inv (1.0) == 0.0);
  }

  /* ── det_dwell_gauss / det_threshold_gauss ───────────────────────── */
  {
    /* Definitional: both formulas re-derived from det_q_inv itself, so a
     * transcription error in either implementation shows up here. */
    double mean = 0.4, var = 0.5, pd = 0.99, pfa = 1e-5;
    double qa = det_q_inv (pfa), qd = det_q_inv (pd), sep = qa - qd;
    DP_CHECK (det_dwell_gauss (mean, var, pd, pfa)
              == (int)ceil (var * (sep / mean) * (sep / mean)));
    DP_CHECK (
        CLOSE (det_threshold_gauss (mean, pd, pfa), qa * mean / sep, 1e-12));

    /* Q_inv(pd) is NEGATIVE above the median, so the separation is a sum
     * of two tails. Getting that sign wrong halves the dwell, which is
     * the transcription error worth pinning explicitly. */
    DP_CHECK (qd < 0.0 && sep > qa);

    /* Equivalence with symsync's shipped erfcinv-direct convention:
     *   avgs = 2*var*((erfcinv(2*pfa) - erfcinv(2*pd))/mean)^2
     * The sqrt(2) cancels in the threshold and the leading 2 absorbs it
     * in the dwell, so the two forms are the SAME formula. This pins
     * that, so the promotion cannot silently move symsync's numbers. */
    {
      double ea = det_q_inv (pfa) * M_SQRT1_2; /* == erfcinv(2*pfa) */
      double ed = det_q_inv (pd) * M_SQRT1_2;  /* == erfcinv(2*pd)  */
      double avgs_symsync
          = 2.0 * var * ((ea - ed) / mean) * ((ea - ed) / mean);
      double thr_symsync = ea * mean / (ea - ed);
      DP_CHECK (CLOSE (avgs_symsync, var * (sep / mean) * (sep / mean), 1e-9));
      DP_CHECK (
          CLOSE (thr_symsync, det_threshold_gauss (mean, pd, pfa), 1e-12));
    }

    /* Scaling laws: the threshold is linear in the mean and blind to the
     * variance; the dwell falls as mean^2 and rises with var. */
    DP_CHECK (CLOSE (det_threshold_gauss (0.8, pd, pfa),
                     2.0 * det_threshold_gauss (0.4, pd, pfa), 1e-12));
    DP_CHECK (det_threshold_gauss (mean, pd, pfa)
              == det_threshold_gauss (mean, pd, pfa));
    DP_CHECK (det_dwell_gauss (0.8, var, pd, pfa)
              < det_dwell_gauss (0.4, var, pd, pfa));
    DP_CHECK (det_dwell_gauss (mean, 2.0 * var, pd, pfa)
              > det_dwell_gauss (mean, var, pd, pfa));
    /* A stricter budget costs looks. */
    DP_CHECK (det_dwell_gauss (mean, var, 0.999, pfa)
              > det_dwell_gauss (mean, var, 0.99, pfa));
    DP_CHECK (det_dwell_gauss (mean, var, pd, 1e-9)
              > det_dwell_gauss (mean, var, pd, 1e-5));

    /* Fail closed on every invalid input, and never return 0 looks. */
    DP_CHECK (det_dwell_gauss (0.0, var, pd, pfa) == -1);  /* no signal   */
    DP_CHECK (det_dwell_gauss (mean, 0.0, pd, pfa) == -1); /* no noise    */
    DP_CHECK (det_dwell_gauss (mean, var, pfa, pd) == -1); /* pd <= pfa   */
    DP_CHECK (det_dwell_gauss (mean, var, 1.0, pfa) == -1);
    DP_CHECK (det_dwell_gauss (mean, var, pd, 0.0) == -1);
    DP_CHECK (det_dwell_gauss (1e6, var, pd, pfa) >= 1); /* clamps, not 0 */
    DP_CHECK (det_threshold_gauss (0.0, pd, pfa) == 0.0);
    DP_CHECK (det_threshold_gauss (mean, pfa, pd) == 0.0);
  }

  /* ── det_threshold_noncoherent / det_pd_noncoherent / det_n_noncoh ─────
   *
   * The non-coherent trio, in C. It was covered only by
   * src/doppler/detection/tests/test_detection.py -- real coverage, in the
   * wrong language for the object whose header is the SSOT, and these three
   * are what acq_core.c sizes its entire (M, N_nc) split with
   * (acq_core.c:237, :275, :308). */
  {
    const double pfa = 1e-3;

    /* Reduce to the coherent helpers EXACTLY at one look. The header states
     * this as an equality, not an approximation, for both functions. */
    for (double p = 1e-6; p < 1e-1; p *= 100.0)
      DP_CHECK (det_threshold_noncoherent (p, 1) == det_threshold (p));
    {
      double eta = det_threshold (1e-6);
      DP_CHECK (det_pd_noncoherent (0.5, 8, 1, eta) == det_pd (0.5, 8, eta));
      DP_CHECK (det_pd_noncoherent (0.0, 8, 1, eta) == det_pd (0.0, 8, eta));
    }

    /* The threshold SOLVES what it claims to solve: eta_nc is the b with
     * marcum_q(n_noncoh, 0, b) == pfa. Measured against marcum_q itself,
     * which is independently pinned above -- not against a literal. */
    for (int nc = 1; nc <= 16; nc *= 2)
      {
        double eta = det_threshold_noncoherent (pfa, nc);
        DP_CHECK (CLOSE (marcum_q (nc, 0.0, eta), pfa, 1e-9));
      }

    /* It GROWS with the look count -- §3 of docs/design/detection.md, and
     * the reason det_n_noncoh must re-derive it every iteration rather than
     * size against a fixed threshold. */
    {
      double prev = det_threshold_noncoherent (pfa, 1);
      for (int nc = 2; nc <= 32; nc *= 2)
        {
          double eta = det_threshold_noncoherent (pfa, nc);
          DP_CHECK (eta > prev);
          prev = eta;
        }
    }

    /* Pd at zero SNR is the per-test Pfa, for every look count. */
    for (int nc = 2; nc <= 8; nc *= 2)
      {
        double eta = det_threshold_noncoherent (pfa, nc);
        DP_CHECK (CLOSE (det_pd_noncoherent (0.0, 16, nc, eta), pfa, 1e-9));
      }

    /* Monotone in SNR, and bounded. */
    {
      double eta = det_threshold_noncoherent (pfa, 4);
      DP_CHECK (det_pd_noncoherent (0.2, 16, 4, eta)
                < det_pd_noncoherent (0.4, 16, 4, eta));
      DP_CHECK (det_pd_noncoherent (0.4, 16, 4, eta)
                < det_pd_noncoherent (0.8, 16, 4, eta));
      DP_CHECK (det_pd_noncoherent (10.0, 16, 4, eta) <= 1.0);
    }

    /* det_n_noncoh returns the MINIMUM look count meeting pd_min -- the
     * value one below must fail, at ITS OWN threshold (the whole point:
     * the threshold moves with the count, so re-deriving it is what makes
     * the minimality claim meaningful). */
    {
      const double snr = 0.25, pd_min = 0.9;
      const int    n_coh = 16;
      int          k     = det_n_noncoh (snr, n_coh, pd_min, pfa, 256);
      DP_CHECK (k > 1);
      if (k > 1)
        {
          DP_CHECK (det_pd_noncoherent (snr, n_coh, k,
                                        det_threshold_noncoherent (pfa, k))
                    >= pd_min);
          DP_CHECK (det_pd_noncoherent (snr, n_coh, k - 1,
                                        det_threshold_noncoherent (pfa, k - 1))
                    < pd_min);
        }
      /* A strong signal needs one look; an impossible one is refused. */
      DP_CHECK (det_n_noncoh (2.0, 16, 0.9, pfa, 64) == 1);
      DP_CHECK (det_n_noncoh (1e-4, 1, 0.99, 1e-9, 4) == -1);
      /* Weaker signal, more looks. */
      DP_CHECK (det_n_noncoh (0.25, 16, 0.9, pfa, 256)
                > det_n_noncoh (0.40, 16, 0.9, pfa, 256));
    }
  }

  /* ── det_threshold_f: the header's 41x, re-derived ────────────────────
   *
   * Nothing here is a defect. A threshold built on a noise reference
   * estimated from n samples inherits that estimate's uncertainty --
   * sigma_hat^2 = sum Im^2 / n is sigma^2*chi2(n)/n, good to sqrt(2/n) --
   * and det_threshold_f exists precisely because doppler prices that
   * correctly. What was missing is that the header quotes one number for
   * it ("41x at n = 16, pfa = 1e-3") and no code re-derived it, so the
   * figure could drift from the arithmetic with no gate noticing.
   *
   * The ratio is a multiplier on the false-alarm RATE, which is not a
   * cost and cannot be added to a link budget. The number a caller
   * actually budgets is the same shortfall taken in the threshold
   * instead -- 3.27 dB at n = 16, about 1 dB at n = 100 -- and that table
   * lives in the certification report (section 2.4), which regenerates it
   * rather than quoting it.
   *
   * THE DEGREES OF FREEDOM ARE THE TRAP, so the derivation comes first --
   * a comparator picked because it reproduces 41 would be fitting, not
   * measuring. Under H0 a burst's n prompts give sum Re^2 ~ s^2*chi2(n)
   * and sum Im^2 ~ s^2*chi2(n), so R^2 = n*F(n,n), which is the header's
   * law. A caller who treats sum Im^2 / n as though it were exactly s^2
   * believes R^2 ~ chi2(n) instead, and gates at that distribution's
   * upper-pfa quantile. det_threshold_noncoherent(pfa, M) solves
   * marcum_q(M, 0, b) = pfa, which IS P(chi2(2M) > b^2) -- so 2M = n and
   * the comparator sits at M = n/2. Pricing it at M = n gives 4.8x here:
   * a plausible number, off by almost ten.
   *
   * EVEN n ONLY, enforced below rather than left to the sweep to avoid.
   * `n / 2` is integer division, so at odd n this prices chi2(n-1) -- the
   * same gate as n-1 -- and overstates the penalty by about a fifth (89x
   * against a true 73.7x at n = 5). doppler cannot express an odd-dof
   * known-noise gate at all, while det_threshold_f handles odd n
   * deliberately, because a burst's prompt count is whatever the burst
   * contained. So the COMPARISON is an even-n statement even though the
   * gate it recommends is not.
   *
   * Cross-checked against scipy while this was written, as a third
   * opinion sharing no code with either: det_threshold_noncoherent(pfa,
   * n/2) matches chi2(n).isf(pfa) to 1e-15, det_threshold_f matches
   * f.isf(pfa, n, n), and f.sf(eta^2/n, n, n) gives the same 41.0x. */
  {
    const double pfa = 1e-3;
    /* Invert det_threshold_f: it is monotone DECREASING in pfa, so bisect
     * on the geometric mean to find the pfa a given quantile really buys. */
    for (int n = 4; n <= 64; n *= 4)
      {
        DP_CHECK (n % 2 == 0); /* see the even-n note above */
        double eta = det_threshold_noncoherent (pfa, n / 2); /* chi2(n) */
        double g   = eta * eta / (double)n;                  /* F units */
        double lo = 1e-15, hi = 0.9999;
        for (int it = 0; it < 200; it++)
          {
            double mid = sqrt (lo * hi);
            if (det_threshold_f (mid, n) > g)
              lo = mid;
            else
              hi = mid;
          }
        double realized = sqrt (lo * hi);

        /* The correct gate is always the stricter one ... */
        DP_CHECK (det_threshold_f (pfa, n) > g);
        /* ... and the chi-square gate is off by the ratio the header
         * quotes: tens of times, shrinking as the estimate hardens. */
        DP_CHECK (realized > 20.0 * pfa);
        if (n == 16)
          DP_CHECK (CLOSE (realized / pfa, 41.0, 0.5));
      }
    /* Hardening is monotone: more dof, less penalty. */
    {
      double r[3];
      int    ns[3] = { 4, 16, 64 };
      for (int i = 0; i < 3; i++)
        {
          int n = ns[i];
          DP_CHECK (n % 2 == 0);
          double eta = det_threshold_noncoherent (pfa, n / 2);
          double g   = eta * eta / (double)n;
          double lo = 1e-15, hi = 0.9999;
          for (int it = 0; it < 200; it++)
            {
              double mid = sqrt (lo * hi);
              if (det_threshold_f (mid, n) > g)
                lo = mid;
              else
                hi = mid;
            }
          r[i] = sqrt (lo * hi);
        }
      DP_CHECK (r[0] > r[1] && r[1] > r[2]);
    }
  }

  /* ── the even-dof restriction, pinned rather than described ───────────
   *
   * det_threshold_noncoherent takes a LOOK COUNT and prices chi2(2M), so
   * there is no argument that yields an odd-dof gate: M = n/2 and
   * M = (n-1)/2 are the same integer for odd n, and the helper returns the
   * same threshold for both. That is what makes the 41x comparison above
   * an even-n statement, and it is asserted here so the restriction cannot
   * quietly stop being true -- a future helper that DID accept odd dof
   * would take this red, which is the correct signal to widen the section
   * above rather than a regression. */
  {
    const double pfa = 1e-3;
    for (int n = 3; n <= 17; n += 2)
      {
        DP_CHECK (det_threshold_noncoherent (pfa, n / 2)
                  == det_threshold_noncoherent (pfa, (n - 1) / 2));
        /* ... and the even neighbours genuinely differ, so the equality
           above is the collapse and not a constant function. */
        DP_CHECK (det_threshold_noncoherent (pfa, (n + 1) / 2)
                  > det_threshold_noncoherent (pfa, (n - 1) / 2));
      }
  }

  /* ── marcum_q across its STATED envelope (a, b <= 15) ──────────────────
   *
   * The header claims the Poisson-weighted series "converges in ~60 terms
   * for practical a, b <= 15". Every value pinned above sits at a <= 3 --
   * comfortably inside, and blind to a series that stops converging near
   * the edge of what the header promises. Measured against Monte-Carlo,
   * which is external to the series in a way another closed form would not
   * be: draw Rice(a, 1) directly and count exceedances.
   *
   * The tolerance is 5 binomial sigma at N draws, so it cannot flake: at
   * p ~ 0.5 that is 5*sqrt(0.25/N) = 0.0079 for N = 100k. */
  {
    const int    N        = 100000;
    const double pts[][2] = { { 8.0, 8.0 }, { 12.0, 14.0 }, { 15.0, 15.0 } };
    for (size_t i = 0; i < sizeof pts / sizeof pts[0]; i++)
      {
        const double a = pts[i][0], b = pts[i][1];
        uint32_t     st   = 12345u + (uint32_t)i * 777u;
        int          hits = 0;
        for (int k = 0; k < N; k++)
          {
            /* Rice(a, 1): |a + n|, n complex with unit variance per
             * component. Named locals -- two dp_gauss calls inside one
             * expression would evaluate in the compiler's chosen order. */
            double nr = dp_gauss (&st);
            double ni = dp_gauss (&st);
            double re = a + nr, im = ni;
            if (sqrt (re * re + im * im) > b)
              hits++;
          }
        double mc  = (double)hits / (double)N;
        double q   = marcum_q (1, a, b);
        double tol = 5.0 * sqrt (q * (1.0 - q) / (double)N) + 1e-6;
        DP_CHECK (fabs (mc - q) < tol);
      }
    /* Still a probability at the very edge, and still monotone there --
     * a series that has stopped converging typically fails one of these
     * before it fails the Monte-Carlo comparison. */
    for (int m = 1; m <= 4; m++)
      {
        double q = marcum_q (m, 15.0, 15.0);
        DP_CHECK (q >= 0.0 && q <= 1.0);
        DP_CHECK (marcum_q (m, 15.0, 15.0) > marcum_q (m, 15.0, 15.5));
      }
  }

  DP_TEST_END ("test_detection_core");
}
