/**
 * @file dp_test.h
 * @brief The foundation of the `dp_*_test.h` family: assertions and the
 *        pass/fail epilogue every C test shares.
 *
 * This file is the floor the rest of the family was already standing on.
 * `dp_state_test.h` says so in its own header — *"Requires (already present
 * in every test_*_core.c): the CHECK macro"* — and that requirement was met
 * by **90 copies of `CHECK` in 6 mutually incompatible variants**: two
 * different arities, two different failure semantics, and one copy whose
 * condition was inverted. A shared harness resting on a macro that every
 * includer redefines for itself is a contract with nobody.
 *
 * ## The two flavours, and why there are exactly two
 *
 * The 90 copies disagreed about what a failed assertion should DO, and both
 * answers were legitimate:
 *
 * - 80 files **accumulated** — record the failure, keep going, report the
 *   count at the end. You learn about all ten broken cases in one run.
 * - 9 files **aborted** — `return 1` immediately. Necessary when the checks
 *   that follow would dereference the pointer that just failed to be
 *   non-NULL.
 *
 * So this header keeps both, under the names the wider testing world already
 * uses for exactly this distinction (Catch2, doctest):
 *
 * | macro                   | on failure                        |
 * | ----------------------- | --------------------------------- |
 * | `DP_CHECK(cond)`        | count it, carry on                |
 * | `DP_REQUIRE(cond)`      | report and `return 1` at once     |
 * | `DP_CHECK_MSG(c, msg)`  | as `DP_CHECK`, with your wording  |
 * | `DP_REQUIRE_MSG(c,msg)` | as `DP_REQUIRE`, with your wording|
 *
 * Choosing one and converting everything to it would not have been a
 * refactor: turning an abort into an accumulate lets the checks *after* a
 * failure run, and several of them exist precisely because the pointer is
 * known-good by then. Keeping both flavours makes the migration provably
 * behaviour-preserving, and leaves "should this file abort or accumulate?"
 * as a question each test answers on its own schedule.
 *
 * ## The counter is file-scope, not a local in main()
 *
 * The copies declared `int _fails = 0;` inside `main`, which works right up
 * until a test grows a helper function — and **20 of doppler's tests call
 * checks from helpers**, which is why 15 of them had already promoted the
 * counter to file scope by hand. Owning it here removes the declaration from
 * every test and the question with it. One translation unit per test
 * executable, so a `static` in a header is exactly right.
 *
 * ## Tolerances are arguments, never constants here
 *
 * The comparison helpers take `tol` rather than reading a `TOL` macro,
 * because `TOL` is defined five times across the suite with five DIFFERENT
 * values (`1e-3f` through `1e-12`) and every one of them is deliberate — a
 * CF32 round-trip and a double-precision spectral estimate do not share an
 * epsilon. Tolerance is a property of the measurement, so it stays at the
 * call site. What was duplicated was the *comparison*, and that lives here.
 *
 * C99, so no `_Generic`: the float and double forms carry distinct names
 * (`dp_nearf` / `dp_near`), the same way the C standard library spells
 * `fabsf` / `fabs`.
 *
 * ## Verbose mode
 *
 * Four tests printed a `PASS` line per check as well as failures. Define
 * `DP_TEST_VERBOSE` before including this header to keep that:
 *
 * @code
 * #define DP_TEST_VERBOSE 1
 * #include "dp_test.h"
 * @endcode
 *
 * ## Usage
 *
 * @code
 * #include "dp_test.h"
 *
 * int
 * main (void)
 * {
 *   thing_t *t = thing_create ();
 *   DP_REQUIRE (t != NULL);            // nothing below is safe without it
 *   DP_CHECK (thing_size (t) == 3);    // independent, so keep going
 *   DP_CHECK (dp_nearf (thing_gain (t), 0.5f, 1e-6f));
 *   thing_destroy (t);
 *   DP_TEST_END ("test_thing_core");
 * }
 * @endcode
 *
 * See native/tests/README.md for the whole family and where new helpers go.
 */
#ifndef DP_TEST_H
#define DP_TEST_H

#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

/* Failure and check counters. `dp_test_checks_` exists so the epilogue can
 * report how much was actually asserted: a test that runs to completion
 * having checked NOTHING passes just as green as one that checked fifty
 * things, which is the failure mode jm's own scaffold-check counter (gh-806)
 * was added upstream to catch. */
static int dp_test_fails_  = 0;
static int dp_test_checks_ = 0;

#ifdef DP_TEST_VERBOSE
#define DP_TEST_PASS_(what) printf ("  PASS  %s\n", (what))
#else
#define DP_TEST_PASS_(what) ((void)0)
#endif

/* The stringified condition carries file and line; a hand-written message
 * carries intent. Both report through one place so the format cannot drift
 * the way six copies of it did. */
#define DP_TEST_FAIL_AT_(what)                                                \
  fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, (what))

#define DP_TEST_FAIL_MSG_(msg) fprintf (stderr, "FAIL: %s\n", (msg))

/** Record a failure and continue. The workhorse: use it unless a later
 *  check would be unsafe once this one has failed. */
#define DP_CHECK(cond)                                                        \
  do                                                                          \
    {                                                                         \
      dp_test_checks_++;                                                      \
      if (!(cond))                                                            \
        {                                                                     \
          DP_TEST_FAIL_AT_ (#cond);                                           \
          dp_test_fails_++;                                                   \
        }                                                                     \
      else                                                                    \
        {                                                                     \
          DP_TEST_PASS_ (#cond);                                              \
        }                                                                     \
    }                                                                         \
  while (0)

/** As DP_CHECK, but reported with your own wording instead of the condition
 *  text. Prefer DP_CHECK: `#cond` cannot go stale, a sentence can. */
#define DP_CHECK_MSG(cond, msg)                                               \
  do                                                                          \
    {                                                                         \
      dp_test_checks_++;                                                      \
      if (!(cond))                                                            \
        {                                                                     \
          DP_TEST_FAIL_MSG_ (msg);                                            \
          dp_test_fails_++;                                                   \
        }                                                                     \
      else                                                                    \
        {                                                                     \
          DP_TEST_PASS_ (msg);                                                \
        }                                                                     \
    }                                                                         \
  while (0)

/** Report and leave main() immediately with a failing status. For the check
 *  that everything after it depends on — a successful create(), a non-NULL
 *  buffer. Only valid where `return 1` is: main(), or a helper returning int.
 */
#define DP_REQUIRE(cond)                                                      \
  do                                                                          \
    {                                                                         \
      dp_test_checks_++;                                                      \
      if (!(cond))                                                            \
        {                                                                     \
          DP_TEST_FAIL_AT_ (#cond);                                           \
          dp_test_fails_++;                                                   \
          return 1;                                                           \
        }                                                                     \
      DP_TEST_PASS_ (#cond);                                                  \
    }                                                                         \
  while (0)

/** As DP_REQUIRE, with your own wording. */
#define DP_REQUIRE_MSG(cond, msg)                                             \
  do                                                                          \
    {                                                                         \
      dp_test_checks_++;                                                      \
      if (!(cond))                                                            \
        {                                                                     \
          DP_TEST_FAIL_MSG_ (msg);                                            \
          dp_test_fails_++;                                                   \
          return 1;                                                           \
        }                                                                     \
      DP_TEST_PASS_ (msg);                                                    \
    }                                                                         \
  while (0)

/**
 * |a-b| <= tol, reported with BOTH values and the tolerance.
 *
 * Lifted from `test_dp_ber.c`, which was the only file to have it and the
 * only one whose numeric failures were diagnosable without a rebuild:
 * `DP_CHECK (dp_near (x, y, tol))` tells you the comparison failed, this
 * tells you it was 0.4713 against 0.5 at tol 0.01. Every numeric assertion
 * in the suite wants the second one.
 */
#define DP_CHECK_NEAR(a, b, tol)                                              \
  do                                                                          \
    {                                                                         \
      double _a = (double)(a), _b = (double)(b);                              \
      dp_test_checks_++;                                                      \
      if (!(fabs (_a - _b) <= (double)(tol)))                                 \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s=%.6g vs %s=%.6g (tol %g)\n",       \
                   __FILE__, __LINE__, #a, _a, #b, _b, (double)(tol));        \
          dp_test_fails_++;                                                   \
        }                                                                     \
      else                                                                    \
        {                                                                     \
          DP_TEST_PASS_ (#a " ~= " #b);                                       \
        }                                                                     \
    }                                                                         \
  while (0)

/**
 * Count a failure you have ALREADY reported yourself.
 *
 * For a helper whose diagnostic is richer than a stringified condition --
 * `test_dp_isotime.c`'s formatters print the arguments, the produced string
 * and the expected one, then need the failure to land in the count. Without
 * this they poked `_fails++` directly, which is exactly the coupling to a
 * caller-owned variable that this header exists to remove.
 *
 * If you have not already printed anything, you want DP_CHECK.
 */
#define DP_RECORD_FAIL()                                                      \
  do                                                                          \
    {                                                                         \
      dp_test_checks_++;                                                      \
      dp_test_fails_++;                                                       \
    }                                                                         \
  while (0)

/**
 * The last statement of main(): report and return.
 *
 * Fails when nothing was checked. A test whose body is `#if 0`-ed out, or
 * whose only loop never ran, otherwise exits 0 and reads as a passing test
 * forever — the same "an empty result set is not a pass" trap the glibc and
 * tarball gates were both caught by. There is no legitimate C test here with
 * zero assertions, so the floor is free.
 *
 * @param name  the test's name, as it should appear in CTest output.
 */
#define DP_TEST_END(name)                                                     \
  do                                                                          \
    {                                                                         \
      if (dp_test_checks_ == 0)                                               \
        {                                                                     \
          fprintf (stderr, "%s ASSERTED NOTHING — no check ran\n", (name));   \
          return 1;                                                           \
        }                                                                     \
      if (dp_test_fails_)                                                     \
        {                                                                     \
          fprintf (stderr, "%s FAILED (%d)\n", (name), dp_test_fails_);       \
          return 1;                                                           \
        }                                                                     \
      printf ("%s PASSED\n", (name));                                         \
      return 0;                                                               \
    }                                                                         \
  while (0)

/* ── Comparisons ─────────────────────────────────────────────────────────
 *
 * Seventeen copies of `_almost_eq`, seventeen of `_almost_eq_c`, five of
 * `_feq`, three of `ceq` — all computing |a-b| <= tol, several with the
 * float/double split done by hand and one pair disagreeing about whether the
 * bound is `<` or `<=`. It is one line of arithmetic; that is exactly why it
 * should not be written eighteen times. */

/** |a-b| <= tol, single precision. */
/**
 * @brief Bits that differ between two packed-octet buffers.
 *
 * The distance a harness scoring RECOVERED PAYLOAD needs: a frame comes back
 * as octets and the question is how many bits of it are wrong. `ber_meter`
 * answers the same question for a symbol stream against a truth sequence and
 * is the right tool there; it cannot be pointed at two byte buffers, and a
 * private popcount loop in each harness that wants one is how two harnesses
 * come to disagree about what a bit error is.
 *
 * @param a  First buffer.
 * @param b  Second buffer.
 * @param n  Octets to compare.
 * @return   Bits that differ, in `[0, 8n]`.
 */
static inline size_t
dp_bit_distance (const uint8_t *a, const uint8_t *b, size_t n)
{
  size_t d = 0;
  for (size_t i = 0; i < n; i++)
    {
      uint8_t x = (uint8_t)(a[i] ^ b[i]);
      while (x)
        {
          d += (size_t)(x & 1u);
          x = (uint8_t)(x >> 1);
        }
    }
  return d;
}

static inline int
dp_nearf (float a, float b, float tol)
{
  return fabsf (a - b) <= tol;
}

/** |a-b| <= tol, double precision. */
static inline int
dp_near (double a, double b, double tol)
{
  return fabs (a - b) <= tol;
}

/** Both components within tol, single precision. Compared per-component
 *  rather than by cabsf(a-b), because a component-wise bound is what every
 *  copy in the suite implemented.
 *
 *  It is the LOOSER of the two, not the stricter — this comment said stricter
 *  until `test_dp_test.c` asserted the difference and had to be written the
 *  other way round. The component test accepts a square of side `2*tol`; the
 *  magnitude test accepts a disc of radius `tol`, which sits strictly inside
 *  it. A diagonal error of `(0.4, 0.4)` has magnitude 0.566 and passes here at
 *  `tol = 0.5`. That matters for exactly the errors this suite measures most —
 *  a carrier phase error moves both rails at once. */
static inline int
dp_cnearf (float complex a, float complex b, float tol)
{
  return dp_nearf (crealf (a), crealf (b), tol)
         && dp_nearf (cimagf (a), cimagf (b), tol);
}

/** Both components within tol, double precision. */
static inline int
dp_cnear (double complex a, double complex b, double tol)
{
  return dp_near (creal (a), creal (b), tol)
         && dp_near (cimag (a), cimag (b), tol);
}

#endif /* DP_TEST_H */
