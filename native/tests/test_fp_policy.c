/*
 * test_fp_policy.c — the library's floating-point policy is the one it
 * declares: -ffast-math WITH -fno-finite-math-only (CMakeLists.txt:146 for
 * gcc and clang, :172 for clang-cl).
 *
 * That flag is load-bearing. AccTrace's max/min-hold keep their trace when
 * a frame's bin is NaN (test_acc_trace_core.c), and every `x != x` and
 * isnan() in the library depends on a NaN surviving the optimiser. Under
 * -ffinite-math-only a compiler may assume no value is NaN and fold those
 * tests to false. AccTrace's own pin cannot see that happen: GCC's
 * compare-and-blend keeps the trace either way (measured, #2105).
 *
 * So this canary makes a NaN the compiler cannot see coming and asks the
 * questions finite-math-only folds away, in double and in float. With the
 * flag removed it goes red at -O2 and -O3 under GCC 15 and clang 21
 * (measured, #2105). clang does not fold at -O0, so the target is built at
 * -O2 whatever the build type, and the canary cannot pass vacuously in a
 * Debug build.
 */
#include "dp_test.h"

#include <math.h>

int
main (void)
{
  volatile double z = 0.0;
  const double    n = z / z;
  DP_CHECK (n != n);
  DP_CHECK (isnan (n));
  DP_CHECK (!(n > 1.0) && !(n < 1.0));

  volatile float zf = 0.0f;
  const float    nf = zf / zf;
  DP_CHECK (nf != nf);
  DP_CHECK (isnan (nf));

  DP_TEST_END ("test_fp_policy");
}
