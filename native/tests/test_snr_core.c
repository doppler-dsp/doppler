/*
 * test_snr_core.c — the two stateless Es/N0 estimators.
 *
 * These are two of the three numbers every receiver measurement in this
 * project rests on, and until now neither had a C test: `snr_m2m4_db` was
 * CALLED by test_async_dsss_receiver_core.c and by two harness headers, which
 * exercises it as a tool without ever pinning what it claims. A primitive used
 * everywhere and asserted nowhere is the shape of a defect that surfaces as a
 * receiver result.
 *
 * Every claim below is taken from the declaration in snr/snr_core.h, and each
 * assertion was watched to fail against a deliberately broken estimator before
 * being committed — the tolerances are wide enough not to flake and far too
 * tight to survive a wrong formula.
 *
 * The stimulus is built here rather than drawn from `wfm`: an estimator must
 * be checked against a signal whose Es/N0 is known by CONSTRUCTION, and
 * generating it with the library would make the check partly circular (the
 * same carve-out scripts/.stimulus-sources-allow makes for oracles). The
 * Box-Muller draw below is the reference, not a convenience.
 */
#define _GNU_SOURCE
#include "dp_rng_test.h"
#include "dp_test.h"
#include "snr/snr_core.h"

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
static uint32_t st = 0x2545F491u;

#define NSYM 200000

int
main (void)
{
  static float complex x[NSYM];
  static uint8_t       bits[NSYM];

  /* ── known answer: QPSK at a constructed Es/N0 ─────────────────────────
   *
   * Unit-modulus symbols, so Es = 1 and N0 is the complex noise power: a
   * per-component sigma of sqrt(1/(2*esn0)) puts the block exactly at esn0.
   * Both estimators must read it back. This is the claim everything else in
   * the library leans on -- `-(Es/N0)` is the EVM bound, the SER anchor and
   * the M2M4/data-aided cross-check all at once.
   */
  for (int k = 0; k < 4; k++)
    {
      const double esn0_db = 3.0 + 6.0 * k; /* 3, 9, 15, 21 dB */
      const double esn0    = pow (10.0, esn0_db / 10.0);
      const double sigma   = 1.0 / sqrt (esn0);
      for (size_t i = 0; i < NSYM; i++)
        {
          bits[i]  = (uint8_t)(dp_uni (&st) > 0.5);
          double a = bits[i] ? -1.0 : 1.0;
          x[i] = (float complex)a + (float complex) (sigma * dp_cgauss (&st));
        }
      double blind = snr_m2m4_db (x, NSYM);
      double aided = snr_data_aided_db (x, NSYM, bits, NSYM);
      char   msg[160];
      snprintf (msg, sizeof msg,
                "at Es/N0 %.0f dB: m2m4 %.2f, data-aided %.2f", esn0_db, blind,
                aided);
      DP_REQUIRE_MSG (fabs (blind - esn0_db) < 0.5, msg);
      DP_REQUIRE_MSG (fabs (aided - esn0_db) < 0.5, msg);
      /* They estimate the same quantity by different routes, so they must
         agree far more closely than either agrees with truth. */
      DP_REQUIRE_MSG (fabs (blind - aided) < 0.5, msg);
    }

  /* ── the invariances each one claims ───────────────────────────────────
   *
   * m2m4: "residual phase does not bias the moment-based estimate".
   * data-aided: "scale-invariant ... and polarity-invariant".
   *
   * These are what let either be used on a stream a tracking loop has left at
   * an arbitrary rotation and an AGC at an arbitrary level. Each is asserted
   * to a much tighter bound than the accuracy above, because the SAME samples
   * are being re-measured -- any drift here is bias, not sampling noise.
   */
  {
    const double esn0_db = 12.0;
    const double sigma   = pow (10.0, -esn0_db / 20.0);
    for (size_t i = 0; i < NSYM; i++)
      {
        bits[i]  = (uint8_t)(dp_uni (&st) > 0.5);
        double a = bits[i] ? -1.0 : 1.0;
        x[i] = (float complex)a + (float complex) (sigma * dp_cgauss (&st));
      }
    double base_blind = snr_m2m4_db (x, NSYM);
    double base_aided = snr_data_aided_db (x, NSYM, bits, NSYM);

    static float complex y[NSYM];
    const float complex  rot = (float complex) (cos (0.7) + sin (0.7) * I);
    for (size_t i = 0; i < NSYM; i++)
      y[i] = x[i] * rot;
    DP_REQUIRE_MSG (fabs (snr_m2m4_db (y, NSYM) - base_blind) < 0.05,
                    "m2m4: a residual rotation must not move the estimate");

    for (size_t i = 0; i < NSYM; i++)
      y[i] = x[i] * 37.5f;
    DP_REQUIRE_MSG (fabs (snr_data_aided_db (y, NSYM, bits, NSYM) - base_aided)
                        < 0.05,
                    "data-aided: scale-invariant");
    DP_REQUIRE_MSG (fabs (snr_m2m4_db (y, NSYM) - base_blind) < 0.05,
                    "m2m4: scale-invariant too (a ratio of moments)");

    for (size_t i = 0; i < NSYM; i++)
      y[i] = -x[i];
    DP_REQUIRE_MSG (fabs (snr_data_aided_db (y, NSYM, bits, NSYM) - base_aided)
                        < 0.05,
                    "data-aided: a global sign flip changes nothing");

    /* The data-aided estimator is data-AIDED: hand it the wrong bits and it
       must NOT keep reporting a healthy link. This is the property that makes
       it a cross-check on m2m4 rather than a second copy of it. */
    static uint8_t wrong[NSYM];
    for (size_t i = 0; i < NSYM; i++)
      wrong[i] = (uint8_t)(dp_uni (&st) > 0.5);
    double misfed = snr_data_aided_db (x, NSYM, wrong, NSYM);
    char   m2[128];
    snprintf (m2, sizeof m2, "wrong bits still read %.2f dB", misfed);
    DP_REQUIRE_MSG (misfed < 1.0, m2);
  }

  /* ── the edges the header names ────────────────────────────────────────
   *
   * "0-linear for pure noise, +inf for a noiseless constant-modulus signal,
   * NaN if x_len is 0 or the block has zero power" -- and for the data-aided
   * form, "NaN if that count is 0 or the residual power is exactly 0". Each
   * is a documented contract, so each is a test; a NaN that silently became a
   * number would read as a measurement.
   */
  {
    for (size_t i = 0; i < NSYM; i++)
      x[i] = dp_cgauss (&st); /* unit-power noise, no signal */
    double npure = snr_m2m4_db (x, NSYM);
    char   m3[96];
    snprintf (m3, sizeof m3, "pure noise reads %.2f dB", npure);
    /* 0 linear is -inf dB; the estimator floors rather than diverging, so the
       assertion is "far below any operating point", not an exact value. */
    DP_REQUIRE_MSG (npure < -10.0 || isinf (npure), m3);

    for (size_t i = 0; i < NSYM; i++)
      x[i] = (i & 1) ? 1.0f : -1.0f; /* noiseless BPSK */
    double clean = snr_m2m4_db (x, NSYM);
    snprintf (m3, sizeof m3, "noiseless reads %.2f dB", clean);
    DP_REQUIRE_MSG (clean > 40.0 || isinf (clean), m3);

    DP_REQUIRE_MSG (isnan (snr_m2m4_db (x, 0)), "m2m4: empty block is NaN");
    for (size_t i = 0; i < 64; i++)
      x[i] = 0.0f;
    DP_REQUIRE_MSG (isnan (snr_m2m4_db (x, 64)), "m2m4: zero power is NaN");

    DP_REQUIRE_MSG (isnan (snr_data_aided_db (x, 0, bits, 0)),
                    "data-aided: empty block is NaN");
    for (size_t i = 0; i < 64; i++)
      {
        bits[i] = 0;
        x[i]    = 1.0f; /* exactly the nominal symbol: zero residual */
      }
    DP_REQUIRE_MSG (isnan (snr_data_aided_db (x, 64, bits, 64)),
                    "data-aided: zero residual power is NaN");

    /* "over min(soft_len, sign_bits_len) paired samples" -- the shorter array
       decides, and a mismatch must not read past either. */
    for (size_t i = 0; i < 4096; i++)
      {
        bits[i]  = (uint8_t)(dp_uni (&st) > 0.5);
        double a = bits[i] ? -1.0 : 1.0;
        x[i]     = (float complex)a + (float complex) (0.1 * dp_cgauss (&st));
      }
    double full   = snr_data_aided_db (x, 4096, bits, 4096);
    double short_ = snr_data_aided_db (x, 4096, bits, 2048);
    DP_REQUIRE_MSG (fabs (full - short_) < 1.0,
                    "data-aided: pairs over min(len), same link either way");
  }

  printf ("test_snr_core: OK (known answer 3/9/15/21 dB, rotation & scale & "
          "polarity invariance, wrong-bits rejection, empty/zero/noiseless "
          "edges, min-length pairing)\n");
  return 0;
}
