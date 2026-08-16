/**
 * @file test_dp_test.c
 * @brief The test for the tests: `dp_test.h`'s own self-test.
 *
 * `dp_test.h` is the assertion foundation for **97** C test files. Nothing
 * tested it. That is the worst place in the tree for an untested thing to
 * sit, because the failure mode is not a red suite — it is a GREEN one:
 * a `DP_CHECK` that records no failure turns every one of those 97 files
 * into a program that runs to completion and reports success, and `ctest`
 * says 100%.
 *
 * That is not hypothetical for this header. Its own docstrings record what it
 * replaced: "90 copies of `CHECK` in six incompatible variants -- two arities,
 * two failure semantics, and one whose condition was INVERTED", and twenty
 * copies whose failure gate had drifted above later assertions, so "75 checks
 * printed FAIL and their tests still exited 0". The consolidation fixed those.
 * Nothing has been watching the replacement.
 *
 * ## How a self-test asserts that an assertion FAILS
 *
 * The obvious problem is circularity: a deliberate `DP_CHECK (0)` would fail
 * this test too. It is resolved by OBSERVING the counters rather than the exit
 * status. `dp_test_fails_` and `dp_test_checks_` are file-scope statics, so
 * this translation unit can snapshot them, run a check that is meant to fail,
 * measure the deltas, restore them, and only then assert -- with a real
 * `DP_CHECK` -- that the deltas were right. The deliberate failure never
 * reaches the verdict.
 *
 * Touching those two variables directly is the one thing `DP_RECORD_FAIL()`
 * exists to stop callers doing. It is correct HERE and nowhere else: this file
 * is the header's instrument, and an instrument has to reach inside the thing
 * it measures.
 *
 * ## And why stderr is captured rather than muted
 *
 * A deliberate failure prints `FAIL ...` to stderr. Left alone it would put
 * fake FAIL lines into the CTest log of a PASSING test, which is its own trap
 * -- someone greps for FAIL, or a future log-scraping gate does. So stderr is
 * redirected into a temp file for the duration.
 *
 * Capturing rather than discarding also buys the other half of the contract:
 * the DIAGNOSTIC. A check that records the failure but prints nothing useful
 * is still broken, just quietly -- the file, the line, the stringified
 * condition and (for DP_CHECK_NEAR) both values and the tolerance are what
 * make a failure diagnosable without a rebuild, and each is asserted here.
 *
 * `DP_TEST_END`'s three exit paths cannot be tested in-process, because it
 * RETURNS. They are covered at process level by `test_dp_test_end.c`.
 */
#include "dp_test.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ── Capturing what the header prints ───────────────────────────────────── */

static FILE *cap_file  = NULL;
static int   cap_saved = -1;

/** @brief Point stderr at a temp file. POSIX `dup2`; doppler is linux/macos
 * (`[project] platforms`), so there is no third case to carry. */
static void
cap_begin (void)
{
  fflush (stderr);
  cap_file  = tmpfile ();
  cap_saved = dup (fileno (stderr));
  if (cap_file && cap_saved >= 0)
    dup2 (fileno (cap_file), fileno (stderr));
}

/** @brief Restore stderr and return what was written, NUL-terminated. */
static const char *
cap_end (char *buf, size_t n)
{
  size_t got = 0;
  buf[0]     = '\0';
  fflush (stderr);
  if (cap_saved >= 0)
    {
      dup2 (cap_saved, fileno (stderr));
      close (cap_saved);
      cap_saved = -1;
    }
  if (cap_file)
    {
      rewind (cap_file);
      got      = fread (buf, 1, n - 1, cap_file);
      buf[got] = '\0';
      fclose (cap_file);
      cap_file = NULL;
    }
  return buf;
}

/* ── Observing a check without being scored by it ───────────────────────── */

/** @brief What one deliberately-run check did. */
typedef struct
{
  int  d_checks; /**< how much `dp_test_checks_` moved */
  int  d_fails;  /**< how much `dp_test_fails_` moved  */
  char out[512]; /**< what it wrote to stderr          */
} probe_t;

/* Snapshot, capture, run the body, measure, restore. The restore is what
   keeps a deliberate failure out of this test's own verdict. */
#define PROBE(p, body)                                                        \
  do                                                                          \
    {                                                                         \
      int _c0 = dp_test_checks_, _f0 = dp_test_fails_;                        \
      cap_begin ();                                                           \
      body;                                                                   \
      cap_end ((p).out, sizeof (p).out);                                      \
      (p).d_checks    = dp_test_checks_ - _c0;                                \
      (p).d_fails     = dp_test_fails_ - _f0;                                 \
      dp_test_checks_ = _c0;                                                  \
      dp_test_fails_  = _f0;                                                  \
    }                                                                         \
  while (0)

/** @brief Did @p p's output mention @p needle? */
static int
said (const probe_t *p, const char *needle)
{
  return strstr (p->out, needle) != NULL;
}

/* ── The side-effect counter, for single-evaluation ─────────────────────── */

static int eval_count = 0;

static int
counted (int v)
{
  eval_count++;
  return v;
}

/* ── DP_REQUIRE returns from its enclosing function ─────────────────────── */

/** @brief Returns 1 via DP_REQUIRE when @p pass is 0, else reaches the end. */
static int
require_helper (int pass, int *reached_end)
{
  DP_REQUIRE (pass);
  *reached_end = 1;
  return 0;
}

/** @brief The DP_REQUIRE_MSG twin. */
static int
require_msg_helper (int pass, int *reached_end)
{
  DP_REQUIRE_MSG (pass, "the required thing");
  *reached_end = 1;
  return 0;
}

int
main (void)
{
  probe_t p;

  printf ("dp_test.h self-test — the assertion foundation 97 test files "
          "trust\n");

  /* ── DP_CHECK: the workhorse, both outcomes ───────────────────────────── */

  PROBE (p, DP_CHECK (1));
  DP_CHECK_MSG (p.d_checks == 1, "DP_CHECK(true) counts one check");
  DP_CHECK_MSG (p.d_fails == 0, "DP_CHECK(true) records no failure");
  DP_CHECK_MSG (p.out[0] == '\0', "DP_CHECK(true) prints nothing");

  PROBE (p, DP_CHECK (0));
  DP_CHECK_MSG (p.d_checks == 1, "DP_CHECK(false) counts one check");
  DP_CHECK_MSG (p.d_fails == 1, "DP_CHECK(false) records one failure");
  /* The diagnostic is half the contract: without file, line and the condition
     text a failure is not diagnosable without a rebuild. */
  DP_CHECK_MSG (said (&p, "FAIL"), "DP_CHECK(false) says FAIL");
  DP_CHECK_MSG (said (&p, "test_dp_test.c"), "DP_CHECK(false) names the file");
  DP_CHECK_MSG (said (&p, "0"), "DP_CHECK(false) prints the condition text");

  /* An INVERTED condition is the specific defect this header replaced -- one
     of the 90 copies had it. Assert the polarity both ways round so a flip
     cannot pass. */
  PROBE (p, DP_CHECK (1 == 1));
  DP_CHECK_MSG (p.d_fails == 0, "a true comparison does not fail");
  PROBE (p, DP_CHECK (1 == 2));
  DP_CHECK_MSG (p.d_fails == 1, "a false comparison does fail");

  /* ── DP_CHECK_MSG: same counting, caller's wording ────────────────────── */

  PROBE (p, DP_CHECK_MSG (0, "a bespoke sentence"));
  DP_CHECK_MSG (p.d_checks == 1 && p.d_fails == 1,
                "DP_CHECK_MSG(false) counts a check and a failure");
  DP_CHECK_MSG (said (&p, "a bespoke sentence"),
                "DP_CHECK_MSG(false) prints the caller's wording");

  PROBE (p, DP_CHECK_MSG (1, "not printed"));
  DP_CHECK_MSG (p.d_fails == 0 && p.out[0] == '\0',
                "DP_CHECK_MSG(true) is silent and records nothing");

  /* ── DP_CHECK_NEAR: the boundary is <=, not < ─────────────────────────── */

  PROBE (p, DP_CHECK_NEAR (1.0, 1.5, 0.5));
  DP_CHECK_MSG (p.d_fails == 0, "DP_CHECK_NEAR passes exactly AT the tol");

  PROBE (p, DP_CHECK_NEAR (1.0, 1.5, 0.4999));
  DP_CHECK_MSG (p.d_fails == 1, "DP_CHECK_NEAR fails just outside the tol");
  DP_CHECK_MSG (said (&p, "1.5") && said (&p, "0.4999"),
                "DP_CHECK_NEAR prints both values and the tolerance");

  /* NaN must FAIL rather than slip through: `fabs(NaN-x) <= tol` is false, so
     the polarity of the guard is what decides this, and a rewrite that
     inverted it would make every NaN a pass. */
  PROBE (p, DP_CHECK_NEAR (0.0 / 0.0, 1.0, 1e9));
  DP_CHECK_MSG (p.d_fails == 1, "DP_CHECK_NEAR fails on NaN, never passes it");

  /* ── DP_RECORD_FAIL: counted, and silent because the caller printed ───── */

  PROBE (p, DP_RECORD_FAIL ());
  DP_CHECK_MSG (p.d_checks == 1 && p.d_fails == 1,
                "DP_RECORD_FAIL counts a check and a failure");
  DP_CHECK_MSG (p.out[0] == '\0',
                "DP_RECORD_FAIL prints nothing — the caller already did");

  /* ── Single evaluation ────────────────────────────────────────────────── */

  eval_count = 0;
  PROBE (p, DP_CHECK (counted (1)));
  DP_CHECK_MSG (eval_count == 1, "DP_CHECK evaluates its condition once");

  eval_count = 0;
  PROBE (p, DP_CHECK (counted (0)));
  DP_CHECK_MSG (eval_count == 1,
                "DP_CHECK evaluates once on the failing path too");

  /* A double-evaluated argument in DP_CHECK_NEAR would silently run a
     measurement twice and compare two different draws -- which reads as a
     flaky receiver rather than a broken macro. */
  eval_count = 0;
  PROBE (p, DP_CHECK_NEAR (counted (1), 1.0, 0.5));
  DP_CHECK_MSG (eval_count == 1, "DP_CHECK_NEAR evaluates each side once");

  /* ── The macros are single statements ─────────────────────────────────── */

  /* Every one is `do { } while (0)`. If any were a bare block, an unbraced
     `if (x) DP_CHECK (...); else ...` would not compile, and the whole suite
     writes them that way. This is a COMPILE-time property, so reaching this
     line at all is the assertion; the runtime check keeps it non-vacuous. */
  {
    int taken = 0;
    if (1)
      DP_CHECK (1);
    else
      taken = 1;
    DP_CHECK_MSG (taken == 0,
                  "the macros are single statements: unbraced if/else binds "
                  "the else correctly");
  }

  /* ── DP_REQUIRE leaves the enclosing function ─────────────────────────── */

  {
    int reached = 0, rc = 0;
    PROBE (p, rc = require_helper (0, &reached));
    DP_CHECK_MSG (rc == 1, "DP_REQUIRE(false) returns 1 from its function");
    DP_CHECK_MSG (reached == 0,
                  "DP_REQUIRE(false) does not run what follows it");
    DP_CHECK_MSG (p.d_checks == 1 && p.d_fails == 1,
                  "DP_REQUIRE(false) counts a check and a failure");

    reached = 0;
    PROBE (p, rc = require_helper (1, &reached));
    DP_CHECK_MSG (rc == 0 && reached == 1,
                  "DP_REQUIRE(true) falls through to the next statement");
    DP_CHECK_MSG (p.d_checks == 1 && p.d_fails == 0,
                  "DP_REQUIRE(true) counts a check and no failure");

    reached = 0;
    PROBE (p, rc = require_msg_helper (0, &reached));
    DP_CHECK_MSG (rc == 1 && reached == 0,
                  "DP_REQUIRE_MSG(false) also returns 1 immediately");
    DP_CHECK_MSG (said (&p, "the required thing"),
                  "DP_REQUIRE_MSG(false) prints the caller's wording");
  }

  /* ── The comparison helpers ───────────────────────────────────────────── */

  /* `|a-b| <= tol`, and the header records that among the eighteen hand-rolled
     copies these replaced, "one pair disagreed about whether the bound is `<`
     or `<=`". So the boundary is the assertion, not the interior. */
  DP_CHECK_MSG (dp_nearf (1.0f, 1.5f, 0.5f), "dp_nearf is inclusive at tol");
  DP_CHECK_MSG (!dp_nearf (1.0f, 1.5f, 0.4999f), "dp_nearf fails past tol");
  DP_CHECK_MSG (dp_near (1.0, 1.5, 0.5), "dp_near is inclusive at tol");
  DP_CHECK_MSG (!dp_near (1.0, 1.5, 0.4999), "dp_near fails past tol");

  /* The complex pair is COMPONENT-WISE, not `cabs(a-b) <= tol`, and the
     header says so deliberately. Both are asserted, because the difference is
     invisible on an axis-aligned error and decides the verdict on a diagonal
     one -- which is every carrier-phase error there is. */
  DP_CHECK_MSG (dp_cnearf (0.0f, 0.5f, 0.5f),
                "dp_cnearf is inclusive at tol on the real rail");
  DP_CHECK_MSG (!dp_cnearf (0.0f, 0.5001f, 0.5f),
                "dp_cnearf fails past tol on the real rail");
  DP_CHECK_MSG (!dp_cnearf (0.0f, 0.5001f * I, 0.5f),
                "dp_cnearf fails past tol on the IMAGINARY rail too");
  DP_CHECK_MSG (dp_cnear (0.0, 0.5, 0.5),
                "dp_cnear is inclusive at tol on the real rail");
  DP_CHECK_MSG (!dp_cnear (0.0, 0.5001 * I, 0.5),
                "dp_cnear fails past tol on the imaginary rail");

  /* The distinction itself, pinned: `(0.4, 0.4)` has magnitude 0.566, so a
     magnitude bound at tol 0.5 would REJECT it and the component bound
     ACCEPTS it. Component-wise is therefore the LOOSER of the two -- its
     acceptance region is a square of side 2*tol, and the magnitude test's
     disc of radius tol sits strictly inside it. Anything that "simplified"
     these to `cabs(a-b) <= tol` would tighten every complex assertion in the
     suite at once, so the semantics are asserted rather than assumed. */
  DP_CHECK_MSG (dp_cnear (0.0, 0.4 + 0.4 * I, 0.5),
                "dp_cnear accepts a diagonal error a magnitude bound would "
                "reject: it is component-wise, and looser");
  DP_CHECK_MSG (cabs (0.4 + 0.4 * I) > 0.5,
                "...and that error really does exceed tol in magnitude");

  /* NaN is not near anything, including itself. A comparison that returned
     true here would make every NaN-producing kernel look correct. */
  DP_CHECK_MSG (!dp_near (0.0 / 0.0, 0.0 / 0.0, 1e9),
                "dp_near says NaN is not near NaN");
  DP_CHECK_MSG (!dp_nearf (0.0f / 0.0f, 1.0f, 1e9f),
                "dp_nearf says NaN is not near a number");

  /* ── The counters mean what the epilogue reads ────────────────────────── */

  /* DP_TEST_END's zero-assertion floor is only as good as this counter, and
     it is the one thing every check above has in common. */
  DP_CHECK_MSG (dp_test_checks_ > 0, "the check counter actually accumulated");

  /* And PROBE itself must not leak, or every deliberate failure above would
     be scoring this test. Asserted directly: snapshot, run a check that
     fails, and confirm both counters came back to where they were. */
  {
    int c0 = dp_test_checks_, f0 = dp_test_fails_;
    int c1, f1;
    PROBE (p, DP_CHECK (0));
    /* Read into locals FIRST. Every check macro does `dp_test_checks_++`
       BEFORE evaluating its condition, so a condition that reads the counter
       directly sees the incremented value and compares against the wrong
       number -- which is how this assertion failed on its first run. */
    c1 = dp_test_checks_;
    f1 = dp_test_fails_;
    DP_CHECK_MSG (c1 == c0 && f1 == f0,
                  "PROBE restores both counters, so a deliberate failure "
                  "cannot leak into this test's own verdict");
  }

  /* That ordering pinned on its own, because anyone writing a check whose
     condition inspects the counters will meet it: the increment happens
     first, so inside the condition the counter is already +1. */
  {
    int before = dp_test_checks_;
    DP_CHECK_MSG (dp_test_checks_ == before + 1,
                  "the check counter increments BEFORE the condition is "
                  "evaluated, not after");
  }

  DP_TEST_END ("test_dp_test");
}
