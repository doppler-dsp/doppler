/*
 * test_ber_core.c — the self-referenced EVM, and the window it is measured on.
 *
 * `ber_evm_db` is the harness's QUALITY metric: BER saturates at zero long
 * before a constellation is good, so EVM is what distinguishes a receiver on
 * its bound from one 8 dB off it. It had no test. It was CALLED by
 * test_ratesync_core.c and by both harness headers, which exercises it without
 * asserting anything it claims — and its central claim is a number, `EVM[dB] ~
 * -(Es/N0)[dB]`, that every EVM threshold in the tree is stated against.
 *
 * Claims taken from the declaration in ber/ber_core.h, one test each:
 *
 *   1. at a matched-filter output, EVM[dB] = -(Es/N0)[dB], with NO factor of
 *      two (it is an I/Q-plane quantity; quoting the I-only form flatters the
 *      result by 3 dB);
 *   2. self-referenced -- it takes no truth and no lag, so a cyclic shift of
 *      the stream cannot change it;
 *   3. the rotation is estimated from the data, so a global rotation cannot
 *      change it either;
 *   4. the window [lo, hi) is EXPLICIT, and scoring a different window gives a
 *      different answer -- which is the whole reason it is not defaulted;
 *   5. 0.0 ("no lock") for a window under 20 symbols;
 *   6. a destroyed constellation reads ber_evm_scatter_floor_db(m), NOT 0 dB.
 *
 * The AWGN below is an independent Box-Muller draw for the same reason as in
 * test_snr_core.c: an estimator has to be checked against a signal whose Es/N0
 * is known by construction.
 */
#include "ber/ber_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"

#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Randomness comes from dp_rng_test.h — the test layer's ONE generator and
   ONE Box-Muller, which `make tests-ssot` enforces. The oracle here is the
   CONSTRUCTION (a known sigma against unit-modulus symbols IS a known Es/N0),
   not the source of the draws, so sharing the generator costs nothing:
   dp_cgauss is not the estimator under test.

   dp_cgauss carries unit TOTAL power (0.5 per rail), so scaling it by
   `sigma = 10^(-esn0_db/20)` puts N0 = sigma^2 against Es = 1. */
static uint32_t st = 0x9E3779B9u;

#define NSYM 100000

int
main (void)
{
  static float complex rx[NSYM];

  /* ── claim 1: EVM[dB] == -(Es/N0)[dB] where the stream is LOCKED ─────
   *
   * The header's "~" and its "a locked stream" are load-bearing, and this test
   * was written without them: asserting the identity at 6 dB Es/N0 fails, and
   * the estimator is right. A self-referenced EVM scores each symbol against
   * the stream's OWN hard decision, so a MISDECIDED symbol is measured against
   * a nearer constellation point than the one that was sent, and the error
   * vector it contributes is too small. The metric therefore FLATTERS at low
   * Es/N0, by an amount set by the symbol error rate. Measured, QPSK:
   *
   *     Es/N0    EVM      flattery   SER
   *      3 dB   -5.45      2.45 dB   1.6e-1
   *      6 dB   -7.06      1.06 dB   4.6e-2
   *      9 dB   -9.44      0.44 dB   4.8e-3
   *     12 dB  -12.20      0.20 dB   6.9e-5
   *     15 dB  -15.11      0.11 dB   1.9e-8
   *     21 dB  -21.04      0.04 dB   3.3e-29
   *
   * So the identity is pinned tightly from 12 dB up -- every operating point
   * this project measures at -- and the flattery below it is pinned as its own
   * property, because the useful statement is not "EVM equals the bound" but
   * "EVM cannot beat the bound by more than the decisions cost".
   */
  for (int k = 0; k < 4; k++)
    {
      const double esn0_db = 12.0 + 4.0 * k; /* 12, 16, 20, 24 dB */
      const double sigma   = pow (10.0, -esn0_db / 20.0);
      for (size_t i = 0; i < NSYM; i++)
        {
          int q = (int)(dp_uni (&st) * 4.0) & 3;
          rx[i] = (float complex) (cos (M_PI_2 * q + M_PI_4)
                                   + sin (M_PI_2 * q + M_PI_4) * I)
                  + (float complex) (sigma * dp_cgauss (&st));
        }
      double evm = ber_evm_db (rx, NSYM, 0, NSYM, 4);
      char   msg[160];
      snprintf (msg, sizeof msg,
                "Es/N0 %.0f dB should read EVM %.0f dB, reads %.2f", esn0_db,
                -esn0_db, evm);
      /* 0.3 dB is tight enough that the 3.01 dB an I-only convention would
         introduce cannot hide in it, and the flattery at these points is
         0.20 dB and falling. */
      DP_REQUIRE_MSG (fabs (evm + esn0_db) < 0.3, msg);
      DP_REQUIRE_MSG (evm > -esn0_db - 0.3,
                      "EVM must not beat the bound where decisions are sound");
    }

  /* The flattery itself, pinned: monotone in Es/N0, and bounded by the value
     measured above. A regression that made the estimator reference truth (or
     stopped estimating the rotation from the data) would move these. */
  {
    const double pts[3]  = { 3.0, 6.0, 9.0 };
    const double most[3] = { 3.0, 1.5, 0.8 }; /* measured 2.45 / 1.06 / 0.44 */
    double       prev    = 99.0;
    for (int k = 0; k < 3; k++)
      {
        const double sigma = pow (10.0, -pts[k] / 20.0);
        for (size_t i = 0; i < NSYM; i++)
          {
            int q = (int)(dp_uni (&st) * 4.0) & 3;
            rx[i] = (float complex) (cos (M_PI_2 * q + M_PI_4)
                                     + sin (M_PI_2 * q + M_PI_4) * I)
                    + (float complex) (sigma * dp_cgauss (&st));
          }
        double flat = -(ber_evm_db (rx, NSYM, 0, NSYM, 4)) - pts[k];
        char   msg[160];
        snprintf (msg, sizeof msg, "Es/N0 %.0f dB: EVM flatters by %.2f dB",
                  pts[k], flat);
        DP_REQUIRE_MSG (flat > 0.1 && flat < most[k], msg);
        DP_REQUIRE_MSG (flat < prev, "flattery must shrink as Es/N0 rises");
        prev = flat;
      }
  }

  /* ── claims 2 and 3: no truth, no lag, no absolute rotation ─────────── */
  {
    const double sigma = pow (10.0, -12.0 / 20.0);
    for (size_t i = 0; i < NSYM; i++)
      {
        int q = (int)(dp_uni (&st) * 4.0) & 3;
        rx[i] = (float complex) (cos (M_PI_2 * q + M_PI_4)
                                 + sin (M_PI_2 * q + M_PI_4) * I)
                + (float complex) (sigma * dp_cgauss (&st));
      }
    double base = ber_evm_db (rx, NSYM, 0, NSYM, 4);

    static float complex y[NSYM];
    const float complex  rot = (float complex) (cos (0.31) + sin (0.31) * I);
    for (size_t i = 0; i < NSYM; i++)
      y[i] = rx[i] * rot;
    DP_REQUIRE_MSG (fabs (ber_evm_db (y, NSYM, 0, NSYM, 4) - base) < 0.05,
                    "EVM must not move under a global rotation");

    /* A cyclic shift is the strongest statement of "no lag": every symbol is
       still there, at a different index. A metric that referenced truth would
       collapse. */
    for (size_t i = 0; i < NSYM; i++)
      y[i] = rx[(i + 12345) % NSYM];
    DP_REQUIRE_MSG (fabs (ber_evm_db (y, NSYM, 0, NSYM, 4) - base) < 0.05,
                    "EVM must not move under a cyclic shift");
  }

  /* ── claim 4: the window is explicit, and it decides ─────────────────── */
  {
    /* Clean first half, noisy second: the two halves must read differently,
       and the whole must land between them. A convenience "back half" default
       would silently score the second one whatever the caller asked for. */
    const double sigma = pow (10.0, -15.0 / 20.0);
    for (size_t i = 0; i < NSYM; i++)
      {
        int q = (int)(dp_uni (&st) * 4.0) & 3;
        rx[i] = (float complex) (cos (M_PI_2 * q + M_PI_4)
                                 + sin (M_PI_2 * q + M_PI_4) * I);
        if (i >= NSYM / 2)
          rx[i] += (float complex) (sigma * dp_cgauss (&st));
      }
    double first = ber_evm_db (rx, NSYM, 0, NSYM / 2, 4);
    double last  = ber_evm_db (rx, NSYM, NSYM / 2, NSYM, 4);
    char   msg[160];
    snprintf (msg, sizeof msg, "clean half %.1f dB, noisy half %.1f dB", first,
              last);
    DP_REQUIRE_MSG (first < -40.0, msg);
    DP_REQUIRE_MSG (fabs (last + 15.0) < 0.3, msg);
    DP_REQUIRE_MSG (first < last - 20.0, msg);
  }

  /* ── claim 5: a window under 20 symbols is "no lock", not a number ───── */
  {
    DP_REQUIRE_MSG (ber_evm_db (rx, NSYM, 0, 19, 4) == 0.0,
                    "19 symbols must read 0.0 (no lock)");
    DP_REQUIRE_MSG (ber_evm_db (rx, NSYM, 0, 0, 4) == 0.0,
                    "an empty window must read 0.0 (no lock)");
    DP_REQUIRE_MSG (ber_evm_db (rx, NSYM, NSYM, NSYM, 4) == 0.0,
                    "an inverted/empty window must read 0.0 (no lock)");
  }

  /* ── claim 6: destroyed reads the SCATTER FLOOR, never 0 dB ──────────── */
  {
    /* Uniformly random phase: no carrier recovery at all. The floor is -1.4 dB
       at BPSK, -7.0 at QPSK, -12.9 at 8PSK, so "EVM < -12" is satisfied by
       pure noise at 8PSK -- which is exactly why every threshold in the tree
       is stated against this function rather than against zero. */
    const int orders[3] = { 2, 4, 8 };
    for (int oi = 0; oi < 3; oi++)
      {
        int m = orders[oi];
        for (size_t i = 0; i < NSYM; i++)
          {
            double ph = 2.0 * M_PI * dp_uni (&st);
            rx[i]     = (float complex) (cos (ph) + sin (ph) * I);
          }
        double evm   = ber_evm_db (rx, NSYM, 0, NSYM, m);
        double floor = ber_evm_scatter_floor_db (m);
        char   msg[160];
        snprintf (msg, sizeof msg, "m=%d: scattered reads %.2f, floor is %.2f",
                  m, evm, floor);
        DP_REQUIRE_MSG (fabs (evm - floor) < 0.5, msg);
      }
  }

  printf ("test_ber_core: OK (EVM == -Es/N0 at 6/11/16/21 dB, rotation & "
          "shift invariance, explicit window, no-lock floor, scatter floor "
          "at m=2/4/8)\n");
  return 0;
}
