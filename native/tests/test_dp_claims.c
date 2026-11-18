/**
 * @file test_dp_claims.c
 * @brief The claim scope of dp_test.h (DP_CLAIM, DP_CONTROL, DP_REFUSAL),
 *        one process per behaviour.
 *
 * claims-gate: exempt (its failing modes break the claim rules on purpose)
 *
 * The epilogue returns, so each path gets a process, selected by argv, and
 * CTest asserts the exit status (WILL_FAIL for the modes that must fail).
 * `src/doppler/tests/test_dp_claims.py` runs the passing modes with
 * DP_CLAIMS_OUT set and reads the records they write.
 *
 *   pass              exits 0; two claims, a control and a pair each
 *   unscoped          exits 0; a check outside any claim, as before; no
 * records helper            exits 0; a helper's checks attach to the open
 * claim control_passes    exits non-zero: the wrong case satisfies the
 * assertion pair_refused      exits non-zero: the minimal pair was refused too
 *   not_refused       exits non-zero: the bad call was accepted
 *   empty_claim       exits non-zero: a claim that checks nothing
 *   outside           exits non-zero: a control outside any claim
 *   duplicate         exits non-zero: one claim id used twice
 */
#include "dp_test.h"

#include <string.h>

static int
twice (int x)
{
  return 2 * x;
}

/* Two checks made from a helper while a claim is open. */
static void
helper_checks (void)
{
  DP_CHECK (twice (2) == 4);
  DP_CHECK (twice (3) == 6);
}

int
main (int argc, char **argv)
{
  const char *mode = (argc > 1) ? argv[1] : "";

  if (strcmp (mode, "pass") == 0)
    {
      DP_CLAIM ("doubles", "twice doubles its argument");
      DP_CHECK (twice (3) == 6);
      DP_CONTROL ("a function that adds one", 3 + 1 == 6);
      DP_CLAIM ("refuses", "a \"negative\" argument is refused");
      DP_REFUSAL (twice (-1) < 0, twice (1) > 0, "the argument made positive");
      DP_TEST_END ("test_dp_claims[pass]");
    }
  if (strcmp (mode, "unscoped") == 0)
    {
      DP_CHECK (twice (2) == 4);
      DP_TEST_END ("test_dp_claims[unscoped]");
    }
  if (strcmp (mode, "helper") == 0)
    {
      DP_CLAIM ("from_helper", "checks made in a helper belong to the claim");
      helper_checks ();
      DP_CONTROL ("a wrong product", twice (3) == 7);
      DP_TEST_END ("test_dp_claims[helper]");
    }
  if (strcmp (mode, "control_passes") == 0)
    {
      DP_CLAIM ("vacuous", "the control's wrong case satisfies the assertion");
      DP_CHECK (twice (3) == 6);
      DP_CONTROL ("a wrong case that is really right", twice (3) == 6);
      DP_TEST_END ("test_dp_claims[control_passes]");
    }
  if (strcmp (mode, "pair_refused") == 0)
    {
      DP_CLAIM ("bad_pair", "the argument made good is refused too");
      DP_REFUSAL (twice (-1) < 0, twice (-1) > 0, "the pair is also bad");
      DP_TEST_END ("test_dp_claims[pair_refused]");
    }
  if (strcmp (mode, "not_refused") == 0)
    {
      DP_CLAIM ("accepts", "the bad call was accepted");
      DP_REFUSAL (twice (1) < 0, twice (1) > 0, "the argument made good");
      DP_TEST_END ("test_dp_claims[not_refused]");
    }
  if (strcmp (mode, "empty_claim") == 0)
    {
      DP_CHECK (1); /* so the zero-assertion floor is not what fires */
      DP_CLAIM ("empty", "a claim that checks nothing");
      DP_TEST_END ("test_dp_claims[empty_claim]");
    }
  if (strcmp (mode, "outside") == 0)
    {
      DP_CHECK (1);
      DP_CONTROL ("a control with no claim", 0);
      DP_TEST_END ("test_dp_claims[outside]");
    }
  if (strcmp (mode, "duplicate") == 0)
    {
      DP_CLAIM ("same", "first");
      DP_CHECK (1);
      DP_CLAIM ("same", "second");
      DP_CHECK (1);
      DP_TEST_END ("test_dp_claims[duplicate]");
    }

  DP_TEST_ERR ("test_dp_claims: unknown mode '%s'\n", mode);
  return 2;
}
