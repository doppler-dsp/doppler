/**
 * @file test_dp_sym.c
 * @brief `dp_sym_test.h`'s self-test: the truth-free symbol-quality layer.
 *
 * Five test files score every receiver in this project through this header,
 * and nothing tested it. It is the layer that decides what "the constellation
 * is good" means, and its failure mode is the one its own docstrings were
 * written to prevent: a plausible number rather than an error.
 *
 * ## What is actually asserted here, and why it is the NUMBERS
 *
 * This header is thin — most entries forward to `ber_core.h` or `snr_core.h`
 * — so testing the arithmetic again would be testing the wrong thing. What is
 * load-bearing is the set of **numeric claims its docstrings make**, because
 * other tests write fixed thresholds against them:
 *
 *   - the scatter floor is `-1.4 / -7.0 / -12.9 dB` at M = 2/4/8;
 *   - a locked stream reads `EVM[dB] ~ -(Es/N0)[dB]`, with no factor of two;
 *   - the room between "on the bound" and "completely broken" is
 *     `5.4 / 3.3 / 2.8 dB` and shrinks with M;
 *   - the settling budget is `2*(5/bn_t + 5/bn_c)` — the two loops ADD, then
 *     the sum DOUBLES.
 *
 * Each of those is a number some other file's assertion is calibrated
 * against. If one drifts, that file keeps passing and starts meaning
 * something else. So they are pinned here, at the source.
 *
 * ## The claim worth the Monte Carlo
 *
 * The header records a live defect: a `< -12.0 dB` EVM assertion "is
 * meaningless at 8PSK — a stream with no carrier recovery at all passes it",
 * and that assertion was in the real receiver's every-M loop until
 * 2026-07-27 (then `test_mpsk_receiver_r_core.c`, since folded into
 * `test_mpsk_receiver_core.c` section 16). That is not a statement about the
 * closed form; it is a statement about what a DESTROYED stream measures. So it
 * is measured: symbols at uniformly random phase, scored, and checked to land
 * on the floor — and then checked to pass the exact threshold the header says
 * it wrongly passes.
 *
 * A closed form agreeing with itself would not have caught that. Generating
 * the scattered stream is the only way the two halves are independent.
 *
 * Randomness comes from `dp_rng_test.h`, the suite's one generator — never a
 * private one (`check_tests_ssot.py` rule 4).
 */
#include "dp_rng_test.h"
#include "dp_sym_test.h"
#include "dp_test.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/** @brief Symbols per statistical run. Enough that a 0.1 dB tolerance is
 * about the estimator and not about the sample count. */
#define NSYM 40000u

/**
 * @brief The scatter floor from first principles, independently of the tree.
 *
 * Slicing a unit-modulus point at a uniformly random phase to its nearest of
 * `m` neighbours leaves a phase error uniform on `[-pi/m, pi/m)`, so
 * `E|e|^2 = E|1 - e^{j@theta}|^2 = 2 - 2 sin(pi/m)/(pi/m)`. Written out here
 * rather than called, because a test that calls the same function it is
 * checking establishes nothing.
 */
static double
floor_db_from_theory (int m)
{
  double mm = (m < 2) ? 2.0 : (double)m;
  double x  = M_PI / mm;
  return 10.0 * log10 (2.0 - 2.0 * sin (x) / x);
}

/** @brief One unit-modulus M-PSK point at Gray label @p k. */
static float complex
psk_point (int k, int m)
{
  double th = 2.0 * M_PI * (double)k / (double)m;
  return (float)cos (th) + (float)sin (th) * I;
}

/**
 * @brief A clean M-PSK stream plus complex AWGN at @p esn0_db.
 *
 * `dp_cgauss` delivers unit TOTAL complex power (`E|z|^2 = 1`), so with
 * unit-modulus symbols the noise scale for a given Es/N0 is `10^(-esn0/20)`
 * and the error vector at the decision is exactly that noise — which is what
 * makes `EVM[dB] ~ -(Es/N0)[dB]` the prediction rather than a fit.
 */
static void
make_psk (float complex *dst, size_t n, int m, double esn0_db, uint32_t *st)
{
  double sigma = pow (10.0, -esn0_db / 20.0);
  size_t i;
  for (i = 0; i < n; i++)
    {
      /* Sequenced deliberately: two draws from one state inside a single
         expression are indeterminately sequenced (C11 6.5.2.2p10) and gcc
         and clang pick opposite orders, so the same seed would carry a
         different stream per compiler. `make lint` rejects the one-line
         form; this is the shape it asks for. */
      unsigned      idx = (unsigned)(dp_xs32 (st) % (uint32_t)m);
      float complex nz  = dp_cgauss (st);
      dst[i]            = psk_point ((int)idx, m) + (float)sigma * nz;
    }
}

/** @brief A destroyed constant-modulus stream: unit modulus, phase uniform on
 * [0, 2pi). No carrier recovery at all, which is the thing the floor
 * describes. */
static void
make_scattered (float complex *dst, size_t n, uint32_t *st)
{
  size_t i;
  for (i = 0; i < n; i++)
    {
      double th = 2.0 * M_PI * dp_uni (st);
      dst[i]    = (float)cos (th) + (float)sin (th) * I;
    }
}

int
main (void)
{
  uint32_t       st    = 12345u;
  float complex *buf   = (float complex *)malloc (NSYM * sizeof *buf);
  float complex *b2    = (float complex *)malloc (NSYM * sizeof *b2);
  int            ms[3] = { 2, 4, 8 };
  int            i;

  DP_REQUIRE (buf != NULL && b2 != NULL);

  printf ("dp_sym_test.h self-test — the truth-free symbol-quality layer\n");

  /* ── 1. The scatter floor, against the closed form ────────────────────── */

  for (i = 2; i <= 16; i *= 2)
    DP_CHECK_NEAR (dp_test_evm_scatter_floor_db (i), floor_db_from_theory (i),
                   1e-9);

  /* The three numbers the header QUOTES, and that other files' thresholds are
     written against. Pinned to the value, not just to the formula, because a
     formula change that moved these would silently re-calibrate every one of
     those thresholds. */
  DP_CHECK_NEAR (dp_test_evm_scatter_floor_db (2), -1.4, 0.05);
  DP_CHECK_NEAR (dp_test_evm_scatter_floor_db (4), -7.0, 0.05);
  DP_CHECK_NEAR (dp_test_evm_scatter_floor_db (8), -12.9, 0.05);

  /* `< 2 is treated as 2`, stated on every function that takes m. */
  DP_CHECK_NEAR (dp_test_evm_scatter_floor_db (0),
                 dp_test_evm_scatter_floor_db (2), 1e-12);
  DP_CHECK_NEAR (dp_test_evm_scatter_floor_db (1),
                 dp_test_evm_scatter_floor_db (2), 1e-12);

  /* The floor gets DEEPER with M, which is the whole reason a fixed threshold
     is unsafe: the same number means "broken" at BPSK and "healthy" at 8PSK.
   */
  DP_CHECK (dp_test_evm_scatter_floor_db (8)
            < dp_test_evm_scatter_floor_db (4));
  DP_CHECK (dp_test_evm_scatter_floor_db (4)
            < dp_test_evm_scatter_floor_db (2));

  /* ── 2. A destroyed stream really does READ the floor ─────────────────── */

  /* The closed form agreeing with itself proves nothing. Generate the stream
     the floor describes and measure it. */
  make_scattered (buf, NSYM, &st);
  for (i = 0; i < 3; i++)
    {
      double got  = dp_test_evm_db_hard_m (buf, NSYM, ms[i]);
      double want = dp_test_evm_scatter_floor_db (ms[i]);
      DP_CHECK_NEAR (got, want, 0.30);
    }

  /* And the defect the header records, reproduced: at 8PSK a stream with NO
     carrier recovery passes a `< -12.0 dB` assertion. This is the assertion
     that was live in the real receiver's every-M loop until 2026-07-27
     (test_mpsk_receiver_core.c section 16 today), and it is why any fixed EVM
     threshold must be stated against the floor rather than against 0 dB. */
  DP_CHECK_MSG (dp_test_evm_db_hard_m (buf, NSYM, 8) < -12.0,
                "a fully scattered 8PSK stream PASSES `< -12.0 dB` — which is "
                "why a fixed EVM threshold must be read against the floor");
  /* The same stream at BPSK does not, so the trap is specific to high M and
     the assertion above is not vacuous. */
  DP_CHECK_MSG (dp_test_evm_db_hard_m (buf, NSYM, 2) > -12.0,
                "...and the identical stream fails it at BPSK, so the trap is "
                "M-dependent rather than universal");

  /* ── 3. A locked stream reads -(Es/N0), with NO factor of two ─────────── */

  /* At 20 dB the estimator's self-referencing flattery is ~0.04 dB (it scores
     against the stream's own hard decision, so a misdecided symbol is charged
     against a nearer point), which is why the anchor is taken up here rather
     than at the SER=1e-3 operating point. */
  for (i = 0; i < 3; i++)
    {
      double esn0 = 20.0;
      make_psk (buf, NSYM, ms[i], esn0, &st);
      DP_CHECK_NEAR (dp_test_evm_db_hard_m (buf, NSYM, ms[i]), -esn0, 0.35);
    }

  /* The factor of two, asserted as an absence. An I-only convention would put
     this 3 dB out, and 3 dB is exactly the size of the error the header warns
     "flatters the result". The tolerance above is 0.35 dB, so it cannot
     absorb one. */
  {
    make_psk (buf, NSYM, 4, 15.0, &st);
    DP_CHECK_MSG (fabs (dp_test_evm_db_hard_m (buf, NSYM, 4) + 15.0) < 0.35,
                  "EVM tracks -(Es/N0) with no factor of two");
    DP_CHECK_MSG (fabs (dp_test_evm_db_hard_m (buf, NSYM, 4) + 15.0) < 2.0,
                  "...and is nowhere near the 3 dB an I-only convention adds");
  }

  /* ── 4. "Pass the real m" — the documented mistake, pinned ────────────── */

  /* A clean QPSK stream scored with a BPSK slicer reads ~0 dB however good the
     constellation is, because every symbol off the real axis is charged as
     error. An EVM near 0 dB beside a PASSING error rate is this mistake, not a
     receiver fault -- the header says so, and this is what makes that
     guidance enforceable instead of advisory. */
  make_psk (buf, NSYM, 4, 20.0, &st);
  DP_CHECK_MSG (dp_test_evm_db_hard_m (buf, NSYM, 2) > -3.0,
                "a clean QPSK stream scored as BPSK reads near 0 dB");
  DP_CHECK_MSG (dp_test_evm_db_hard_m (buf, NSYM, 4) < -19.0,
                "...while the same stream scored with the real m is on the "
                "bound");
  /* dp_test_evm_db_hard() is exactly the m = 2 spelling, so the BPSK
     convenience form cannot quietly become something else. */
  DP_CHECK_NEAR (dp_test_evm_db_hard (buf, NSYM),
                 dp_test_evm_db_hard_m (buf, NSYM, 2), 1e-12);

  /* ── 5. The WINDOW is respected, at both ends ─────────────────────────── */

  /* BER and EVM must be scored on the same window; the range form is what
     makes that possible. Garbage outside [lo, hi) must not reach the number —
     if `rx_len` were ever passed as the array length instead of `hi`, or `lo`
     ignored, this is what would notice. */
  {
    double clean, windowed;
    size_t lo = NSYM / 4, hi = NSYM / 2;
    size_t j;
    make_psk (buf, NSYM, 4, 20.0, &st);
    clean = dp_test_evm_db_hard_range (buf, lo, hi, 4);
    /* Scatter everything OUTSIDE the window. */
    for (j = 0; j < NSYM; j++)
      if (j < lo || j >= hi)
        {
          double th = 2.0 * M_PI * dp_uni (&st);
          buf[j]    = (float)(3.0 * cos (th)) + (float)(3.0 * sin (th)) * I;
        }
    windowed = dp_test_evm_db_hard_range (buf, lo, hi, 4);
    DP_CHECK_NEAR (windowed, clean, 1e-9);
    DP_CHECK_MSG (windowed < -19.0,
                  "the windowed EVM is still on the bound, so the assertion "
                  "above is not comparing two broken numbers");
  }

  /* The back-half convenience form scores exactly [n/2, n). Garbage in the
     front half must not reach it -- and it must agree with the range form
     given the same window, or the two spellings mean different things. */
  {
    double back, ranged;
    size_t j;
    make_psk (buf, NSYM, 4, 20.0, &st);
    for (j = 0; j < NSYM / 2; j++)
      buf[j] = 5.0f + 5.0f * I;
    back   = dp_test_evm_db_hard_m (buf, NSYM, 4);
    ranged = dp_test_evm_db_hard_range (buf, NSYM / 2, NSYM, 4);
    DP_CHECK_NEAR (back, ranged, 1e-12);
    DP_CHECK_MSG (back < -19.0,
                  "the back half is scored, not the whole array");
  }

  /* ── 6. The short-stream sentinels ────────────────────────────────────── */

  /* Both return a SENTINEL rather than a plausible number, so a symbol famine
     is obvious instead of being read as a measurement. */
  make_psk (buf, NSYM, 4, 20.0, &st);
  DP_CHECK_NEAR (dp_test_evm_db_hard_m (buf, 19, 4), 0.0, 1e-12);

  /* The BACK-HALF forms need 40, not 20, and their own guard says 20.
     `dp_test_evm_db_hard_m` rejects `n_syms < 20` and then scores `[n/2, n)`
     -- a window of `n/2` -- which `ber_evm_db` in turn rejects below 20. So
     for 20 <= n < 40 the function clears its own guard and returns the
     sentinel from the layer beneath it. The sentinel is honest either way
     (the docstring promises 0.0 "if the stream is too short"), but the
     threshold a reader takes from `n_syms < 20` is roughly half the real one.
     The exact boundary is 39, not 40: the scored window is `n - n/2`, which
     is `ceil(n/2)`, so an ODD length gets one symbol more than halving
     suggests. Asserted at the ACTUAL boundary so it cannot move unnoticed --
     and it is 39/38 rather than 40/39 because this test got it wrong first. */
  DP_CHECK_MSG (dp_test_evm_db_hard_m (buf, 38, 4) == 0.0,
                "the back-half EVM still returns the sentinel at 38 symbols "
                "— it scores ceil(n/2), so its real floor is 39, not the 20 "
                "its own guard names");
  DP_CHECK_MSG (dp_test_evm_db_hard_m (buf, 39, 4) != 0.0,
                "...and produces a number at 39, where the back half is "
                "exactly 20 symbols");
  /* The range form's floor is the honest 20, since it scores what it is
     given -- which is the other half of the argument for preferring it. */
  DP_CHECK_MSG (dp_test_evm_db_hard_range (buf, 0, 19, 4) == 0.0,
                "the range form's floor is 19");
  DP_CHECK_MSG (dp_test_evm_db_hard_range (buf, 0, 20, 4) != 0.0,
                "...and it measures 20 symbols, because it scores exactly "
                "the window it is handed");

  DP_CHECK_NEAR (dp_test_m2m4_snr_db (buf, 19), -120.0, 1e-12);
  /* The identical doubling on the M2M4 pair, for the identical reason. */
  DP_CHECK_MSG (dp_test_m2m4_snr_db (buf, 38) == -120.0,
                "the back-half M2M4 sentinel runs to 38, the same ceil(n/2) "
                "boundary");
  DP_CHECK_MSG (dp_test_m2m4_snr_db (buf, 39) > -120.0,
                "...and produces a number at 39");
  DP_CHECK_NEAR (dp_test_m2m4_snr_db_range (buf, 0, 19), -120.0, 1e-12);
  DP_CHECK_NEAR (dp_test_m2m4_snr_db_range (buf, 100, 100), -120.0, 1e-12);
  DP_CHECK_NEAR (dp_test_m2m4_snr_db_range (buf, 200, 100), -120.0, 1e-12);
  DP_CHECK (dp_test_m2m4_snr_db_range (buf, 0, 4000) > -120.0);

  /* ── 7. M2M4 is INDEPENDENT of the EVM, and recovers Es/N0 ────────────── */

  /* The two are paired everywhere in the suite precisely because they fail
     differently, so the useful assertion is that the blind one lands on the
     stated Es/N0 without ever seeing a decision. */
  for (i = 0; i < 3; i++)
    {
      double esn0 = 15.0;
      make_psk (buf, NSYM, ms[i], esn0, &st);
      DP_CHECK_NEAR (dp_test_m2m4_snr_db (buf, NSYM), esn0, 1.5);
    }

  /* And its back-half form agrees with the explicit window, same as the EVM
     pair -- two spellings, one measurement. */
  {
    make_psk (buf, NSYM, 4, 15.0, &st);
    DP_CHECK_NEAR (dp_test_m2m4_snr_db (buf, NSYM),
                   dp_test_m2m4_snr_db_range (buf, NSYM / 2, NSYM), 1e-12);
  }

  /* Noise-dominated symbols estimate near 0 dB, which is the header's claim
     and the reason a healthy reading is evidence rather than arithmetic. */
  {
    size_t j;
    for (j = 0; j < NSYM; j++)
      b2[j] = dp_cgauss (&st);
    DP_CHECK_MSG (dp_test_m2m4_snr_db (b2, NSYM) < 3.0,
                  "M2M4 on pure noise does not report a healthy link");
  }

  /* ── 8. The settling budget: the loops ADD, then the sum DOUBLES ──────── */

  /* `2*(5/bn_t + 5/bn_c)`. Both halves of the derivation are asserted
     separately, because the recorded cost of getting it wrong is reading from
     `5/bn` alone: -9.0 dB EVM where the settled answer is -23.2 dB. */
  DP_CHECK (dp_test_settle_syms (0.01, 0.005) == (size_t)(2 * (500 + 1000)));

  /* The ADD: two loops cost the sum of their budgets, not the larger. */
  DP_CHECK_MSG (dp_test_settle_syms (0.01, 0.005)
                    == dp_test_settle_syms (0.01, 0.0)
                           + dp_test_settle_syms (0.0, 0.005),
                "the two loops ADD — cascaded, so carrier cannot converge "
                "until timing has");

  /* The DOUBLE: one loop alone is 2*(5/bn), not 5/bn. */
  DP_CHECK (dp_test_settle_syms (0.01, 0.0) == (size_t)(2 * 500));
  DP_CHECK (dp_test_settle_syms (0.0, 0.005) == (size_t)(2 * 1000));

  /* A `bn` of 0 means "that loop is not running", not "divide by zero". */
  DP_CHECK (dp_test_settle_syms (0.0, 0.0) == 0);

  /* It is normalised to the SYMBOL rate, so it is invariant to samples per
     symbol -- there is no sps argument, and that is the property. Halving the
     bandwidth doubles the budget, exactly. */
  DP_CHECK (dp_test_settle_syms (0.005, 0.0)
            == 2 * dp_test_settle_syms (0.01, 0.0));

  free (buf);
  free (b2);
  DP_TEST_END ("test_dp_sym");
}
