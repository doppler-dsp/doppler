/*
 * test_clib_common.c — the checked size product, dp_size_mul() and its
 * saturating face dp_size_mul_sat(), and the double-to-size_t range check
 * dp_size_from_double() (clib_common.h).
 *
 * A size product that wraps allocates small and is then written past.
 * The guard had been hand-written at every site, a different way each
 * time, and one file grew three wrapping products in two review rounds;
 * dp_size_mul is its one home, and scripts/check_size_mul_sites.py keeps
 * it so. Its contract is pinned here once, rather than assumed at every
 * caller:
 *
 *   1. a zero factor is a product of 0, and fits;
 *   2. 1 x SIZE_MAX fits, both ways round;
 *   3. the boundary is exact: SIZE_MAX / b fits and SIZE_MAX / b + 1 does
 *      not, for several b, so an off-by-one guard (`> SIZE_MAX / b + 1`)
 *      or a missing one goes red;
 *   4. a refusal leaves *out untouched;
 *   5. *out may alias a factor, so a product chains in place;
 *   6. dp_size_mul_sat returns SIZE_MAX exactly when dp_size_mul refuses.
 *   7. dp_size_from_double takes [0, 2^w), w the width of size_t, and
 *      nothing else: 2^w itself is refused, which the `(double)SIZE_MAX`
 *      bound it replaced got wrong on LP64 (SIZE_MAX rounds up to 2^64).
 *
 * Each section was proven by sabotage: the guard widened by one and then
 * removed, and this file went red each time; for section 7, the bound
 * loosened to `<= (double)SIZE_MAX`.
 *
 * The factors go through volatile reads so the compiler cannot fold the
 * checks at build time: the function under test runs on every build type.
 */
#include "dp_test.h"

#include "doppler/clib_common.h"
#include <limits.h>
#include <math.h>
#include <stdint.h>

static size_t
opaque (size_t x)
{
  volatile size_t v = x;
  return v;
}

static double
opaque_d (double x)
{
  volatile double v = x;
  return v;
}

int
main (void)
{
  /* 1. A zero factor, either side, fits and is 0. */
  {
    size_t out = 99;
    DP_CHECK (dp_size_mul (opaque (0), opaque (12345), &out) == 1);
    DP_CHECK (out == 0);
    out = 99;
    DP_CHECK (dp_size_mul (opaque (12345), opaque (0), &out) == 1);
    DP_CHECK (out == 0);
    out = 99;
    DP_CHECK (dp_size_mul (opaque (SIZE_MAX), opaque (0), &out) == 1);
    DP_CHECK (out == 0);
    out = 99;
    DP_CHECK (dp_size_mul (opaque (0), opaque (SIZE_MAX), &out) == 1);
    DP_CHECK (out == 0);
  }

  /* 2. 1 x SIZE_MAX is the largest product, and fits both ways round. */
  {
    size_t out = 0;
    DP_CHECK (dp_size_mul (opaque (1), opaque (SIZE_MAX), &out) == 1);
    DP_CHECK (out == SIZE_MAX);
    out = 0;
    DP_CHECK (dp_size_mul (opaque (SIZE_MAX), opaque (1), &out) == 1);
    DP_CHECK (out == SIZE_MAX);
  }

  /* 3 and 4. The boundary, for a power of two, an odd number, a larger
   * power of two and a prime: the quotient fits, one more refuses and
   * leaves *out alone. Both factor orders, since the guard divides by b. */
  {
    static const size_t B[] = { 2, 3, 16, 4093 };
    for (size_t i = 0; i < sizeof B / sizeof B[0]; i++)
      {
        const size_t b  = opaque (B[i]);
        const size_t hi = SIZE_MAX / b;

        size_t out = 0;
        DP_CHECK (dp_size_mul (hi, b, &out) == 1);
        DP_CHECK (out == hi * b);
        DP_CHECK (SIZE_MAX - out < b);
        out = 0;
        DP_CHECK (dp_size_mul (b, hi, &out) == 1);
        DP_CHECK (out == hi * b);

        out = 77;
        DP_CHECK (dp_size_mul (hi + 1, b, &out) == 0);
        DP_CHECK (out == 77);
        out = 77;
        DP_CHECK (dp_size_mul (b, hi + 1, &out) == 0);
        DP_CHECK (out == 77);
      }
  }

  /* 4. A refusal with a factor of SIZE_MAX on both sides leaves *out. */
  {
    size_t out = 1234;
    DP_CHECK (dp_size_mul (opaque (SIZE_MAX), opaque (SIZE_MAX), &out) == 0);
    DP_CHECK (out == 1234);
  }

  /* 5. Chaining: *out aliases the first factor. A fit updates it in place;
   * a refusal mid-chain keeps the last good product. */
  {
    size_t n = opaque (7);
    DP_CHECK (dp_size_mul (n, opaque (3), &n) == 1);
    DP_CHECK (n == 21);
    DP_CHECK (dp_size_mul (n, opaque (2), &n) == 1);
    DP_CHECK (n == 42);
    DP_CHECK (dp_size_mul (n, opaque (SIZE_MAX / 2), &n) == 0);
    DP_CHECK (n == 42);

    size_t m = opaque (5);
    DP_CHECK (dp_size_mul (opaque (4), m, &m) == 1);
    DP_CHECK (m == 20);
  }

  /* 6. The saturating face: the product when it fits, SIZE_MAX exactly at
   * the refusals above. */
  {
    DP_CHECK (dp_size_mul_sat (opaque (6), opaque (7)) == 42);
    DP_CHECK (dp_size_mul_sat (opaque (0), opaque (SIZE_MAX)) == 0);
    DP_CHECK (dp_size_mul_sat (opaque (1), opaque (SIZE_MAX)) == SIZE_MAX);
    const size_t b = opaque (4093);
    DP_CHECK (dp_size_mul_sat (SIZE_MAX / b, b) == (SIZE_MAX / b) * b);
    DP_CHECK (dp_size_mul_sat (SIZE_MAX / b, b) != SIZE_MAX);
    DP_CHECK (dp_size_mul_sat (SIZE_MAX / b + 1, b) == SIZE_MAX);
    DP_CHECK (dp_size_mul_sat (opaque (SIZE_MAX), opaque (2)) == SIZE_MAX);
  }

  /* 7. dp_size_from_double. 2^w is taken from the type's width, spelled
   * differently from the implementation (ldexp, not SIZE_MAX), so the
   * expectations are right on a 32-bit and a 64-bit size_t alike. */
  {
    const double two_w = ldexp (1.0, (int)(CHAR_BIT * sizeof (size_t)));
    const double below = nextafter (two_w, 0.0); /* largest double < 2^w */
    size_t       out   = 0;

    DP_CHECK (dp_size_from_double (opaque_d (0.0), &out) == 1);
    DP_CHECK (out == 0);
    out = 9;
    DP_CHECK (dp_size_from_double (opaque_d (0.5), &out) == 1);
    DP_CHECK (out == 0); /* truncated toward zero */
    out = 9;
    DP_CHECK (dp_size_from_double (opaque_d (-0.0), &out) == 1);
    DP_CHECK (out == 0);

    /* 2^53, the last integer every double holds: in range on LP64, not
     * where size_t is 32 bits. */
    const double p53 = opaque_d (0x1p53);
    out              = 9;
    if (p53 < two_w)
      {
        DP_CHECK (dp_size_from_double (p53, &out) == 1);
        DP_CHECK (out == (size_t)p53);
      }
    else
      {
        DP_CHECK (dp_size_from_double (p53, &out) == 0);
        DP_CHECK (out == 9);
      }

    /* The top of the range, and its edge. */
    out = 9;
    DP_CHECK (dp_size_from_double (opaque_d (below), &out) == 1);
    DP_CHECK (out == (size_t)below);
    DP_CHECK (out > SIZE_MAX / 2);
    out = 9;
    DP_CHECK (dp_size_from_double (opaque_d (two_w), &out) == 0);
    DP_CHECK (out == 9);
    DP_CHECK (dp_size_from_double (opaque_d (0x1p64), &out) == 0);
    DP_CHECK (out == 9);

    /* Refused outright, *out untouched. */
    DP_CHECK (dp_size_from_double (opaque_d (-1.0), &out) == 0);
    DP_CHECK (dp_size_from_double (opaque_d (-0x1p-1074), &out) == 0);
    DP_CHECK (dp_size_from_double (opaque_d (NAN), &out) == 0);
    DP_CHECK (dp_size_from_double (opaque_d (INFINITY), &out) == 0);
    DP_CHECK (dp_size_from_double (opaque_d (-INFINITY), &out) == 0);
    DP_CHECK (out == 9);
  }

  DP_TEST_END ("test_clib_common");
}
