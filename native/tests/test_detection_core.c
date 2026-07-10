#include "detection/detection_core.h"
#include <math.h>
#include <stdio.h>

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

#define CLOSE(a, b, tol) (fabs ((a) - (b)) < (tol))

int
main (void)
{
  int _fails = 0;

  /* ── marcum_q: special cases ─────────────────────────────────────── */

  /* b <= 0 must return 1.0 regardless of a and m. */
  CHECK (marcum_q (1, 0.0, 0.0) == 1.0);
  CHECK (marcum_q (1, 3.0, 0.0) == 1.0);
  CHECK (marcum_q (3, 2.0, -1.0) == 1.0);

  /* ── marcum_q: Q_M(0, b) = exp(-v) * sum_{j=0}^{M-1} v^j/j! ──── */

  /* M=1: Q_1(0, b) = exp(-b^2/2) */
  CHECK (CLOSE (marcum_q (1, 0.0, 1.0), exp (-0.5), 1e-12));
  CHECK (CLOSE (marcum_q (1, 0.0, 2.0), exp (-2.0), 1e-12));

  /* M=2: Q_2(0, b) = exp(-v)*(1 + v),  v = b^2/2 */
  /* v = 2: Q_2(0, 2) = exp(-2)*(1+2) = 3*exp(-2) */
  CHECK (CLOSE (marcum_q (2, 0.0, 2.0), 3.0 * exp (-2.0), 1e-12));

  /* ── marcum_q: values with nonzero a (reference: Python series) ── */
  CHECK (CLOSE (marcum_q (1, 2.0, 1.0), 0.9181076963694063, 1e-10));
  CHECK (CLOSE (marcum_q (1, 1.0, 2.0), 0.2690120600359100, 1e-10));
  CHECK (CLOSE (marcum_q (2, 1.0, 1.0), 0.9407902191465286, 1e-10));
  CHECK (CLOSE (marcum_q (1, 3.0, 2.0), 0.8867207544023923, 1e-10));

  /* ── marcum_q: monotonicity ──────────────────────────────────────── */
  /* Larger a  =>  higher probability (better SNR helps detection). */
  CHECK (marcum_q (1, 1.0, 2.0) < marcum_q (1, 2.0, 2.0));
  /* Larger b  =>  lower probability (harder threshold to cross). */
  CHECK (marcum_q (1, 2.0, 3.0) < marcum_q (1, 2.0, 1.0));
  /* Higher order M (more integration) with same a, b. */
  CHECK (marcum_q (1, 1.5, 1.5) < marcum_q (2, 1.5, 1.5));

  /* ── det_threshold ───────────────────────────────────────────────── */
  /* Roundtrip: exp(-eta^2/2) must recover pfa. */
  {
    double eta4  = det_threshold (1e-4);
    double eta6  = det_threshold (1e-6);
    double eta10 = det_threshold (1e-10);
    CHECK (CLOSE (exp (-0.5 * eta4 * eta4), 1e-4, 1e-14));
    CHECK (CLOSE (exp (-0.5 * eta6 * eta6), 1e-6, 1e-14));
    CHECK (CLOSE (exp (-0.5 * eta10 * eta10), 1e-10, 1e-14));
    /* Known value. */
    CHECK (CLOSE (eta6, 5.256521769756932, 1e-10));
  }

  /* ── det_pd ──────────────────────────────────────────────────────── */
  {
    double eta = det_threshold (1e-6);

    /* At snr=0, Pd must equal Pfa (noise-only regime). */
    CHECK (CLOSE (det_pd (0.0, 1, eta), 1e-6, 1e-14));
    CHECK (CLOSE (det_pd (0.0, 4, eta), 1e-6, 1e-14));

    /* Higher SNR improves Pd. */
    CHECK (det_pd (0.5, 4, eta) < det_pd (1.0, 4, eta));

    /* More dwell improves Pd at fixed SNR. */
    CHECK (det_pd (0.5, 1, eta) < det_pd (0.5, 8, eta));

    /* Pd is bounded in [0, 1]. */
    CHECK (det_pd (10.0, 64, eta) <= 1.0);
    CHECK (det_pd (0.0, 1, eta) >= 0.0);
  }

  /* ── det_dwell ───────────────────────────────────────────────────── */
  {
    /* Very high SNR: single dwell is enough. */
    CHECK (det_dwell (100.0, 0.9, 1e-6, 256) == 1);

    /* Extremely low SNR: cannot meet target within max_dwell. */
    CHECK (det_dwell (0.001, 0.9, 1e-6, 10) == -1);

    /* Returned dwell achieves the target. */
    int m = det_dwell (0.5, 0.9, 1e-6, 512);
    CHECK (m > 0);
    if (m > 0)
      {
        double eta = det_threshold (1e-6);
        CHECK (det_pd (0.5, m, eta) >= 0.9);
        /* Previous dwell should not suffice (minimum dwell property). */
        if (m > 1)
          CHECK (det_pd (0.5, m - 1, eta) < 0.9);
      }
  }

  /* ── det_snr ─────────────────────────────────────────────────────── */
  {
    /* Roundtrip: det_pd at returned SNR must meet pd_min. */
    double snr4  = det_snr (4, 0.9, 1e-6);
    double snr8  = det_snr (8, 0.9, 1e-6);
    double snr16 = det_snr (16, 0.9, 1e-6);
    double eta   = det_threshold (1e-6);

    CHECK (det_pd (snr4, 4, eta) >= 0.9 - 1e-12);
    CHECK (det_pd (snr8, 8, eta) >= 0.9 - 1e-12);
    CHECK (det_pd (snr16, 16, eta) >= 0.9 - 1e-12);

    /* More dwell requires less SNR (coherent gain). */
    CHECK (snr16 < snr8);
    CHECK (snr8 < snr4);

    /* Result is non-negative. */
    CHECK (snr4 >= 0.0);
  }

  /* ── det_threshold_power ─────────────────────────────────────────── */
  {
    /* Roundtrip: exp(-p) must recover pfa. */
    double p4  = det_threshold_power (1e-4);
    double p6  = det_threshold_power (1e-6);
    double p10 = det_threshold_power (1e-10);
    CHECK (CLOSE (exp (-p4), 1e-4, 1e-14));
    CHECK (CLOSE (exp (-p6), 1e-6, 1e-14));
    CHECK (CLOSE (exp (-p10), 1e-10, 1e-14));
    /* Known value: -ln(1e-6) = 6·ln(10). */
    CHECK (CLOSE (p6, 6.0 * log (10.0), 1e-12));
    /* Relationship to amplitude threshold: p = eta^2/2. */
    double eta6 = det_threshold (1e-6);
    CHECK (CLOSE (p6, 0.5 * eta6 * eta6, 1e-12));
  }

  /* ── det_pd_power ────────────────────────────────────────────────── */
  {
    double p   = det_threshold_power (1e-6);
    double eta = det_threshold (1e-6);

    /* At snr_power=0, Pd must equal Pfa. */
    CHECK (CLOSE (det_pd_power (0.0, 1, p), 1e-6, 1e-14));

    /* Equivalence with amplitude detector:
     * det_pd_power(snr^2, M, p) == det_pd(snr, M, eta)
     * because Q_1(sqrt(2M)*snr, eta) == Q_1(sqrt(2M*snr^2), sqrt(2p))
     * and eta = sqrt(2p). */
    CHECK (CLOSE (det_pd_power (4.0, 1, p), det_pd (2.0, 1, eta), 1e-12));
    CHECK (CLOSE (det_pd_power (1.0, 4, p), det_pd (1.0, 4, eta), 1e-12));

    /* Higher snr_power improves Pd. */
    CHECK (det_pd_power (0.5, 4, p) < det_pd_power (2.0, 4, p));
    /* More dwell improves Pd at fixed snr_power. */
    CHECK (det_pd_power (1.0, 1, p) < det_pd_power (1.0, 8, p));
  }

  /* ── det_dwell_power ─────────────────────────────────────────────── */
  {
    /* Equivalence: det_dwell_power(snr^2) == det_dwell(snr). */
    int m_amp = det_dwell (0.5, 0.9, 1e-6, 512);
    int m_pow = det_dwell_power (0.25, 0.9, 1e-6, 512); /* 0.25 = 0.5^2 */
    CHECK (m_amp == m_pow);
    CHECK (m_pow > 0);

    /* Returned dwell achieves the target for power detector. */
    if (m_pow > 0)
      {
        double p = det_threshold_power (1e-6);
        CHECK (det_pd_power (0.25, m_pow, p) >= 0.9);
        if (m_pow > 1)
          CHECK (det_pd_power (0.25, m_pow - 1, p) < 0.9);
      }
  }

  /* ── det_snr_power ───────────────────────────────────────────────── */
  {
    /* Roundtrip: det_pd_power at returned snr_power must meet pd_min. */
    double sp4 = det_snr_power (4, 0.9, 1e-6);
    double sp8 = det_snr_power (8, 0.9, 1e-6);
    double p   = det_threshold_power (1e-6);

    CHECK (det_pd_power (sp4, 4, p) >= 0.9 - 1e-12);
    CHECK (det_pd_power (sp8, 8, p) >= 0.9 - 1e-12);

    /* More dwell requires less power SNR. */
    CHECK (sp8 < sp4);

    /* Equivalence: det_snr_power = det_snr^2. */
    double sa4 = det_snr (4, 0.9, 1e-6);
    CHECK (CLOSE (sp4, sa4 * sa4, 1e-8));
  }

  /* ── det_ema_alpha ───────────────────────────────────────────────── */
  {
    /* No gain requested (or possible): no averaging. */
    CHECK (det_ema_alpha (0.0, 0.0) == 1.0);
    CHECK (det_ema_alpha (10.0, 5.0) == 1.0);

    /* alpha = 2*gin/(gin+gout): 20 dB gain -> 1/alpha = 50.5 regardless
       of where the pair sits on the dB axis (only the gain matters). */
    double a20 = det_ema_alpha (0.0, 20.0);
    CHECK (CLOSE (1.0 / a20, 50.5, 1e-9));
    CHECK (CLOSE (det_ema_alpha (10.0, 30.0), a20, 1e-12));

    /* The forward relation holds: SNR_out = SNR_in * (2 - a) / a. */
    double a    = det_ema_alpha (3.0, 27.0);
    double gain = (2.0 - a) / a;
    CHECK (CLOSE (10.0 * log10 (gain), 24.0, 1e-9));
  }

  if (_fails)
    {
      fprintf (stderr, "test_detection_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_detection_core PASSED\n");
  return 0;
}
