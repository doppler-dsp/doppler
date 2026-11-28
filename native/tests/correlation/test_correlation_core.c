/* test_correlation_core.c — the correlation module's free functions.
 *
 * gh-1034: jm generates and owns a function-only module, and used to
 * generate no C test for it, so the component whose C jm writes end to end
 * was the one with nothing checking it. This module's one free function is
 * the same-signal key, dp_correlator_same_code_phase().
 */
#include "doppler/correlation/correlation_core.h"
#include <math.h>

/* dp_test.h, not jm's scaffold harness: `jm_test.h` is gitignored, so a
 * test including it builds on a machine that has run `jm apply` and fails
 * in CI. doppler keeps ONE assertion harness; `make tests-ssot` is the
 * gate that catches the other one. */
#include "dp_test.h"

/* The key's three header claims, each with the case it rejects:
   - inclusive at DP_CORRELATOR_SAME_PHASE_CHIPS, and two signals just
     past it (the rejected case: 1.5 chips, the false alarm of #3401);
   - circular: phases either side of chip 0 are near, and a pair half a
     code apart is two;
   - symmetric in its two phases.
   The rows are in chips: they pin the header's width, one chip, as well as
   the test. Sabotage: compare without the fold -> red on the
   across-zero rows; `<` for `<=` -> red on the width itself; a key
   widened to 1.5 chips -> red on the 1.5-chip row. */
static int
_test_same_code_phase (void)
{
  const double l = 1023.0;
  static const struct
  {
    double a, db; /* b = a + db, folded into [0, l) */
    int    same;
  } rows[] = {
    { 100.0, 0.0, 1 },  { 100.0, 0.5, 1 },      { 100.0, 1.0, 1 },
    { 100.0, -1.0, 1 }, { 100.0, 1.000001, 0 }, { 100.0, 1.5, 0 },
    { 0.4, -0.6, 1 },   { 0.4, -1.6, 0 },       { 1022.5, 1.0, 1 },
    { 0.0, 511.5, 0 },  { 500.0, 300.0, 0 },
  };
  for (size_t k = 0; k < sizeof rows / sizeof rows[0]; k++)
    {
      const double a = rows[k].a;
      const double b = fmod (a + rows[k].db + l, l);
      DP_CHECK_MSG (dp_correlator_same_code_phase (a, b, l) == rows[k].same,
                    rows[k].same ? "one signal" : "two signals");
      DP_CHECK_MSG (dp_correlator_same_code_phase (b, a, l) == rows[k].same,
                    "the key is symmetric");
    }
  DP_CHECK_MSG (!dp_correlator_same_code_phase (NAN, 0.0, l),
                "a NaN phase is two signals");
  return 0;
}

int
main (void)
{
  DP_CHECK (_test_same_code_phase () == 0);
  DP_TEST_END ("test_correlation_core");
}
