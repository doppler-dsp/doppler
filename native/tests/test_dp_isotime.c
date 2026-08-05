/**
 * @file test_dp_isotime.c
 * @brief Golden vectors pinning dp_isotime.h to just-bashit's
 * `iso-8601-basic`.
 *
 * The format is defined by `just_bashit/datetime.sh`, and code cannot be
 * shared between a bash library and a C one. So the agreement is a fact
 * checked here rather than a claim in a comment. Every expected string below
 * was produced by the helper itself:
 *
 *     source src/just_bashit/datetime.sh
 *     iso-8601-basic -d '2026-08-05T04:15:30.123456789Z' [-m|-u|-n]
 *
 * Regenerate the same way if the helper's contract ever changes; do not
 * hand-edit an expected value to make a failing test pass, since the whole
 * point is that this side follows the other.
 *
 * @note Uses the `_fails` + `CHECK` convention of the other hand-registered
 * tests, **not** `assert`. doppler builds Release, Release defines `NDEBUG`,
 * and `NDEBUG` compiles `assert` away — a first draft of this file used
 * `assert` and reported "all passed" while printing every mismatch, so a
 * deliberately broken formatter still exited 0.
 */
#include "dp_isotime.h"

#include <stdio.h>
#include <string.h>

static int _fails = 0;

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          _fails++;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

/** 2026-08-05T04:15:30Z as seconds since the UNIX epoch. */
#define T_2026 1785903330LL

static void
expect (int64_t sec, uint32_t nsec, unsigned frac, const char *want)
{
  char buf[DP_ISOTIME_MAX];
  int  n = dp_isotime_format (buf, sizeof buf, sec, nsec, frac);
  if (n < 0 || strcmp (buf, want) != 0)
    {
      fprintf (stderr,
               "FAIL %s  dp_isotime_format(%lld, %u, %u) = \"%s\" (%d), "
               "want \"%s\"\n",
               __FILE__, (long long)sec, nsec, frac, n < 0 ? "<error>" : buf,
               n, want);
      _fails++;
      return;
    }
  CHECK ((size_t)n == strlen (want));
}

/* iso-8601-basic -d '2026-08-05T04:15:30.123456789Z' [-m|-u|-n] */
static void
test_golden_vectors (void)
{
  expect (T_2026, 123456789u, DP_ISOTIME_SEC, "20260805T041530Z");
  expect (T_2026, 123456789u, DP_ISOTIME_MSEC, "20260805T041530.123Z");
  expect (T_2026, 123456789u, DP_ISOTIME_USEC, "20260805T041530.123456Z");
  expect (T_2026, 123456789u, DP_ISOTIME_NSEC, "20260805T041530.123456789Z");
}

/* The vector that catches a rounding implementation. The shell helper
   truncates -- verified against it -- so .999888777 is .999 at millisecond
   precision. `(nsec + scale/2) / scale` gives 1000, which formats as
   ".1000Z": a four-digit millisecond field, one second in the future, and
   disagreeing with every filename written beside it. Mutation-tested: this
   function fails if the truncation is replaced by rounding. */
static void
test_fraction_truncates_never_rounds (void)
{
  expect (T_2026, 999888777u, DP_ISOTIME_MSEC, "20260805T041530.999Z");
  expect (T_2026, 999888777u, DP_ISOTIME_USEC, "20260805T041530.999888Z");
  expect (T_2026, 999888777u, DP_ISOTIME_NSEC, "20260805T041530.999888777Z");
  /* Seconds precision must not be nudged by a nearly-whole fraction. */
  expect (T_2026, 999999999u, DP_ISOTIME_SEC, "20260805T041530Z");
}

/* A zero fraction still pads to the requested width -- ".0" is not ".000",
   and a consumer splitting on the dot would mis-parse a short field. */
static void
test_zero_fraction_pads (void)
{
  expect (T_2026, 0u, DP_ISOTIME_MSEC, "20260805T041530.000Z");
  expect (T_2026, 0u, DP_ISOTIME_NSEC, "20260805T041530.000000000Z");
  expect (T_2026, 999u, DP_ISOTIME_USEC, "20260805T041530.000000Z");
}

/* The epoch itself, and pre-epoch instants: `sec` is signed and gmtime_r
   handles negatives, so a 1950s BLUE timecode converted to UNIX time still
   formats rather than returning garbage. */
static void
test_epoch_and_negative (void)
{
  expect (0, 0u, DP_ISOTIME_SEC, "19700101T000000Z");
  expect (-1, 0u, DP_ISOTIME_SEC, "19691231T235959Z");
  /* J1950: 1950-01-01T00:00:00Z, i.e. -631152000 UNIX seconds. */
  expect (-631152000LL, 0u, DP_ISOTIME_SEC, "19500101T000000Z");
}

static void
test_rejects_bad_input (void)
{
  char buf[DP_ISOTIME_MAX];
  /* nsec out of range */
  CHECK (
      dp_isotime_format (buf, sizeof buf, T_2026, 1000000000u, DP_ISOTIME_SEC)
      < 0);
  /* frac must be one of 0/3/6/9 */
  CHECK (dp_isotime_format (buf, sizeof buf, T_2026, 0u, 1u) < 0);
  CHECK (dp_isotime_format (buf, sizeof buf, T_2026, 0u, 12u) < 0);
  CHECK (dp_isotime_format (NULL, sizeof buf, T_2026, 0u, DP_ISOTIME_SEC) < 0);
  /* Too small a buffer reports failure rather than truncating silently. */
  char tiny[8];
  CHECK (dp_isotime_format (tiny, sizeof tiny, T_2026, 0u, DP_ISOTIME_SEC)
         < 0);
}

/* No colons, no path separators -- the entire reason the basic form exists. */
static void
test_is_filename_safe (void)
{
  char buf[DP_ISOTIME_MAX];
  CHECK (
      dp_isotime_format (buf, sizeof buf, T_2026, 123456789u, DP_ISOTIME_NSEC)
      > 0);
  CHECK (strchr (buf, ':') == NULL);
  CHECK (strchr (buf, '/') == NULL);
  CHECK (strchr (buf, '\\') == NULL);
  CHECK (strchr (buf, ' ') == NULL);
}

static void
test_now_is_wellformed (void)
{
  char buf[DP_ISOTIME_MAX];
  int  n = dp_isotime_now (buf, sizeof buf, DP_ISOTIME_MSEC);
  CHECK (n == 20); /* YYYYMMDDThhmmss.fffZ = 15 + 1 + 3 + 1 */
  CHECK (n > 15 && buf[8] == 'T');
  CHECK (n > 15 && buf[15] == '.');
  CHECK (n > 0 && buf[n - 1] == 'Z');
}

int
main (void)
{
  test_golden_vectors ();
  test_fraction_truncates_never_rounds ();
  test_zero_fraction_pads ();
  test_epoch_and_negative ();
  test_rejects_bad_input ();
  test_is_filename_safe ();
  test_now_is_wellformed ();

  if (_fails)
    {
      fprintf (stderr, "test_dp_isotime FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_dp_isotime PASSED\n");
  return 0;
}
