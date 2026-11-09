/*
 * test_clib_common.c — the checked size product, dp_size_mul() and its
 * saturating face dp_size_mul_sat(), and the double-to-size_t range check
 * dp_size_from_double(), and the array allocators built on the product,
 * dp_mallocn() / dp_reallocn() / dp_callocn() / dp_xmallocn() /
 * dp_xreallocn() / dp_xcallocn() (clib_common.h).
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
 *   6b. dp_size_add: the sum when it fits, 0 and *out untouched at the
 *      first sum that wraps, and it chains after dp_size_mul.
 *   7. dp_size_from_double takes [0, 2^w), w the width of size_t, and
 *      nothing else: 2^w itself is refused, which the `(double)SIZE_MAX`
 *      bound it replaced got wrong on LP64 (SIZE_MAX rounds up to 2^64).
 *   8. dp_mallocn allocates n * size when it fits and returns NULL at the
 *      first count that overflows, where `malloc (n * size)` wraps to a
 *      small request and SUCCEEDS; dp_xmallocn / dp_xreallocn allocate
 *      the same sizes, and all three answer a zero count with a live
 *      one-byte allocation, never NULL.
 *   8b. dp_reallocn returns NULL at the first count that overflows, for
 *      several element sizes, and leaves p live with its contents
 *      unchanged; the fitting side of that boundary is section 3's (a
 *      request of ~SIZE_MAX bytes cannot be allocated to prove it). A
 *      zero count is a live block that does not free p, and a grow then
 *      a shrink keeps the prefix.
 *   8c. dp_callocn / dp_xcallocn: the element count n * m is refused at
 *      its first overflowing value, for several m (the fitting side is
 *      section 3's again); an n * m that fits but whose n * m * size does
 *      not is refused by calloc's own check; the block really is zeroed,
 *      over a freed block of the same size filled with a pattern; and a
 *      zero factor, of either count or size, is a live one-byte block.
 *   9. dp_xmallocn, dp_xreallocn and dp_xcallocn ABORT on an overflowing
 *      count: run as `test_clib_common --abort <which>` by three more
 *      ctests, which pass only when SIGABRT is raised.
 *
 * Each section was proven by sabotage: the guard widened by one and then
 * removed, and this file went red each time; for section 7, the bound
 * loosened to `<= (double)SIZE_MAX`; for 8 and 9, the product checked
 * by dp_size_mul replaced with a plain `n * size` (for 8b, in
 * dp_reallocn; for 8c and xcallocn, `n * m` in dp_callocn and
 * dp_xcallocn).
 *
 * The factors go through volatile reads so the compiler cannot fold the
 * checks at build time: the function under test runs on every build type.
 */
#include "dp_test.h"

#include "doppler/clib_common.h"
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

/* Section 8c: malloc a block of nb bytes, fill it with 0xA5 and free it,
 * so the next same-size allocation is likely handed that block back. The
 * stores go through a volatile pointer, or the compiler may drop them as
 * dead before the free. 0 on OOM. */
static int
leave_dirty_block (size_t nb)
{
  unsigned char *d = malloc (nb);
  if (!d)
    return 0;
  volatile unsigned char *v = d;
  for (size_t i = 0; i < nb; ++i)
    v[i] = 0xA5;
  free (d);
  return 1;
}

/* Section 8c asks calloc for an n * size that overflows and expects the
 * NULL the C standard promises. ASan's and TSan's allocators both report
 * that request as a fatal calloc-overflow instead, unless
 * allocator_may_return_null is set; set it for this binary alone, under
 * whichever sanitizer it is built with, so the case runs there too rather
 * than being skipped. *SAN_OPTIONS from the environment still applies on
 * top, and none of the repo's sets this key. */
#if defined(__SANITIZE_ADDRESS__)
#define CLIB_TEST_ASAN 1
#endif
#if defined(__SANITIZE_THREAD__)
#define CLIB_TEST_TSAN 1
#endif
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define CLIB_TEST_ASAN 1
#endif
#if __has_feature(thread_sanitizer)
#define CLIB_TEST_TSAN 1
#endif
#endif
#if defined(CLIB_TEST_ASAN) && !defined(_MSC_VER)
const char *__asan_default_options (void);
const char *
__asan_default_options (void)
{
  return "allocator_may_return_null=1";
}
#endif
#if defined(CLIB_TEST_TSAN) && !defined(_MSC_VER)
const char *__tsan_default_options (void);
const char *
__tsan_default_options (void)
{
  return "allocator_may_return_null=1";
}
#endif

/* Section 9: abort() raises SIGABRT; catching it and leaving with status
 * 0 is the pass. Returning from the allocator at all is the failure. */
static void
on_abort (int sig)
{
  (void)sig;
  _Exit (0);
}

static int
abort_mode (const char *which)
{
  const size_t n = opaque (SIZE_MAX / 2 + 1); /* n * 2 is 2^w: overflows */
#ifdef _MSC_VER
  /* The Debug CRT's abort() opens a report dialog and waits on it, which
     hangs a ctest run; turn the message box and the fault report off so
     SIGABRT is raised straight away, on every build type. */
  _set_abort_behavior (0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  (void)signal (SIGABRT, on_abort);
  void *p = NULL;
  if (strcmp (which, "xmallocn") == 0)
    p = dp_xmallocn (n, 2);
  else if (strcmp (which, "xreallocn") == 0)
    p = dp_xreallocn (NULL, n, 2);
  else if (strcmp (which, "xcallocn") == 0)
    p = dp_xcallocn (n, 2, 1);
  else
    return 2;
  /* Reached only when the overflow was not caught: a wrapped request of 0
     bytes came back as a live allocation. */
  free (p);
  DP_TEST_ERR ("FAIL: dp_%s returned on an overflowing count\n", which);
  return 1;
}

int
main (int argc, char **argv)
{
  if (argc == 3 && strcmp (argv[1], "--abort") == 0)
    return abort_mode (argv[2]);

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

  /* 6b. dp_size_add: exact at the edge, both orders, and chained. */
  {
    size_t out = 5;
    DP_CHECK (dp_size_add (opaque (0), opaque (0), &out) == 1 && out == 0);
    DP_CHECK (dp_size_add (opaque (SIZE_MAX - 7), opaque (7), &out) == 1);
    DP_CHECK (out == SIZE_MAX);
    out = 5;
    DP_CHECK (dp_size_add (opaque (SIZE_MAX - 7), opaque (8), &out) == 0);
    DP_CHECK (dp_size_add (opaque (8), opaque (SIZE_MAX - 7), &out) == 0);
    DP_CHECK (dp_size_add (opaque (SIZE_MAX), opaque (SIZE_MAX), &out) == 0);
    DP_CHECK (out == 5);
    size_t end = 0;
    DP_CHECK (dp_size_mul (opaque (3), opaque (256), &end)
              && dp_size_add (end, opaque (1024), &end));
    DP_CHECK (end == 1792);
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

  /* 8. The array allocators. Each fitting size is allocated and written
   * end to end, so a short buffer is an ASan error as well as a wrong
   * answer. */
  {
    const size_t n = opaque (1000);
    double      *a = dp_mallocn (n, sizeof *a);
    DP_REQUIRE (a != NULL);
    a[n - 1] = 1.0;
    free (a);

    double *b    = dp_xmallocn (n, sizeof *b);
    b[n - 1]     = 1.0;
    b            = dp_xreallocn (b, 2 * n, sizeof *b);
    b[2 * n - 1] = 2.0;
    DP_CHECK (b[n - 1] == 1.0);
    free (b);

    /* The first overflowing count, for an element size of 2 and of 8:
     * `n * size` is exactly 2^w, which wraps to a 0-byte request. */
    DP_CHECK (dp_mallocn (opaque (SIZE_MAX / 2 + 1), 2) == NULL);
    DP_CHECK (dp_mallocn (opaque (SIZE_MAX / 8 + 1), 8) == NULL);
    DP_CHECK (dp_mallocn (opaque (SIZE_MAX), opaque (SIZE_MAX)) == NULL);

    /* A zero count is one byte, never NULL: NULL means overflow or OOM.
     * The realloc case is the one that bites: glibc's realloc (p, 0) frees
     * p and returns NULL, which dp_xnn would read as OOM and abort on. */
    void *z = dp_mallocn (opaque (0), 8);
    DP_CHECK (z != NULL);
    free (z);
    z = dp_mallocn (opaque (8), 0);
    DP_CHECK (z != NULL);
    free (z);
    z = dp_xmallocn (opaque (0), 8);
    DP_CHECK (z != NULL);
    z                = dp_xreallocn (z, opaque (16), 8);
    ((char *)z)[127] = 1;
    z                = dp_xreallocn (z, opaque (0), 8);
    DP_CHECK (z != NULL);
    free (z);
  }

  /* 8b. dp_reallocn: the refusing resize. The block is filled with a
   * known pattern first, so a refusal that freed, moved or rewrote it
   * shows as an ASan error or a wrong byte. */
  {
    const size_t   len = 64;
    unsigned char *p   = dp_reallocn (NULL, opaque (len), 1);
    DP_REQUIRE (p != NULL);
    for (size_t i = 0; i < len; ++i)
      p[i] = (unsigned char)(i + 1);

    /* The boundary, refused side: SIZE_MAX / b + 1 elements of b bytes
     * is the first count whose product wraps (to 0 for a power of two,
     * to 2 for b = 3), which a plain `realloc (p, n * size)` would grant
     * as a tiny block. SIZE_MAX / b itself fits (section 3). */
    static const size_t b[] = { 2, 3, 8, 16, 24 };
    for (size_t k = 0; k < sizeof b / sizeof b[0]; ++k)
      {
        size_t fits;
        DP_CHECK (dp_size_mul (opaque (SIZE_MAX / b[k]), b[k], &fits) == 1);
        DP_CHECK (dp_reallocn (p, opaque (SIZE_MAX / b[k] + 1), b[k]) == NULL);
      }
    DP_CHECK (dp_reallocn (p, opaque (SIZE_MAX), opaque (SIZE_MAX)) == NULL);
    int intact = 1;
    for (size_t i = 0; i < len; ++i)
      intact &= p[i] == (unsigned char)(i + 1);
    DP_CHECK (intact);

    /* Grow, then shrink: the prefix survives both. */
    unsigned char *g = dp_reallocn (p, opaque (4096), 2);
    DP_REQUIRE (g != NULL);
    p       = g;
    p[8191] = 0xAA;
    intact  = 1;
    for (size_t i = 0; i < len; ++i)
      intact &= p[i] == (unsigned char)(i + 1);
    DP_CHECK (intact);
    g = dp_reallocn (p, opaque (len / 2), 1);
    DP_REQUIRE (g != NULL);
    p      = g;
    intact = 1;
    for (size_t i = 0; i < len / 2; ++i)
      intact &= p[i] == (unsigned char)(i + 1);
    DP_CHECK (intact);

    /* A zero count, either factor, is a live one-byte block, never a
     * free: realloc (p, 0) would free p and return NULL. */
    g = dp_reallocn (p, opaque (0), 8);
    DP_REQUIRE (g != NULL);
    p    = g;
    p[0] = 1;
    g    = dp_reallocn (p, opaque (8), 0);
    DP_REQUIRE (g != NULL);
    p = g;
    DP_CHECK (p[0] == 1);
    free (p);
  }

  /* 8c. dp_callocn / dp_xcallocn: a zeroed block of n * m elements. */
  {
    /* The n * m boundary, refused side: SIZE_MAX / m + 1 rows of m is
     * the first count that wraps (to 0 for a power of two, to 2 for
     * m = 3), which `calloc (n * m, size)` would grant as a tiny block.
     * SIZE_MAX / m itself fits (section 3); with size 1 that count is
     * still ~SIZE_MAX bytes, which cannot be allocated to prove it. */
    static const size_t m[] = { 2, 3, 8, 16, 24 };
    for (size_t k = 0; k < sizeof m / sizeof m[0]; ++k)
      {
        size_t fits;
        DP_CHECK (dp_size_mul (opaque (SIZE_MAX / m[k]), m[k], &fits) == 1);
        DP_CHECK (dp_callocn (opaque (SIZE_MAX / m[k] + 1), m[k], 1) == NULL);
      }
    DP_CHECK (dp_callocn (opaque (SIZE_MAX), opaque (SIZE_MAX), 1) == NULL);

    /* n * m fits (SIZE_MAX / 4 * 2 < SIZE_MAX) but n * m * size does
     * not: refused by calloc's own nmemb * size check, which dp_callocn
     * keeps by passing the count and size separately. */
    size_t cnt;
    DP_CHECK (dp_size_mul (opaque (SIZE_MAX / 4), 2, &cnt) == 1);
    DP_CHECK (dp_callocn (opaque (SIZE_MAX / 4), 2, 4) == NULL);

    /* Zeroing really happens: free a same-size block filled with a
     * pattern first, so an allocator that reuses it (glibc's tcache
     * does) would hand the pattern back to a malloc. Every byte of the
     * 3 x 5 floats must read 0. */
    const size_t rows = opaque (3), cols = opaque (5);
    const size_t nb = sizeof (float[3][5]);
    DP_REQUIRE (leave_dirty_block (nb));
    float *z = dp_callocn (rows, cols, sizeof *z);
    DP_REQUIRE (z != NULL);
    const unsigned char *zb    = (const unsigned char *)z;
    int                  clean = 1;
    for (size_t i = 0; i < nb; ++i)
      clean &= zb[i] == 0;
    DP_CHECK (clean);
    free (z);

    DP_REQUIRE (leave_dirty_block (nb));
    z     = dp_xcallocn (rows, cols, sizeof *z);
    zb    = (const unsigned char *)z;
    clean = 1;
    for (size_t i = 0; i < nb; ++i)
      clean &= zb[i] == 0;
    DP_CHECK (clean);
    free (z);

    /* A zero factor -- either half of the count, or the size -- is a
     * live one-byte zeroed block, never NULL. */
    unsigned char *one = dp_callocn (opaque (0), 5, 4);
    DP_REQUIRE (one != NULL);
    DP_CHECK (one[0] == 0);
    free (one);
    one = dp_callocn (5, opaque (0), 4);
    DP_REQUIRE (one != NULL);
    DP_CHECK (one[0] == 0);
    free (one);
    one = dp_callocn (5, 3, opaque (0));
    DP_REQUIRE (one != NULL);
    DP_CHECK (one[0] == 0);
    free (one);
    one = dp_xcallocn (opaque (0), 5, 4);
    DP_CHECK (one != NULL && one[0] == 0);
    free (one);
    one = dp_xcallocn (5, 3, opaque (0));
    DP_CHECK (one != NULL && one[0] == 0);
    free (one);
  }

  DP_TEST_END ("test_clib_common");
}
