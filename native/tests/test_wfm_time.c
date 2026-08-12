/**
 * @file test_wfm_time.c
 * @brief BLUE timecode <-> UNIX epoch, including the cases that make a naive
 * conversion wrong.
 *
 * The offset itself is arithmetic and hard to get wrong once written. What
 * this pins down is the surrounding judgement: that a zero timecode means
 * "unset" rather than 1950, that a pre-1970 capture is real data rather than
 * an error, and that neither of those may quietly become a plausible-looking
 * nanosecond value handed to the sample clock.
 *
 * Uses the `_fails` + `CHECK` convention, not `assert`: doppler builds
 * Release and Release defines NDEBUG, which compiles `assert` away.
 */
#include "dp_test.h"
#include "wfm/wfm_time.h"

#include <stdio.h>

/* `date -u -d 1950-01-01T00:00:00Z +%s` = -631152000, so the offset is the
   negation of that: 20 years with 5 leap days. */
static void
test_offset_is_the_documented_constant (void)
{
  DP_CHECK (WFM_J1950_UNIX_OFFSET_SEC == 631152000.0);
  DP_CHECK (WFM_J1950_UNIX_OFFSET_SEC == 7305.0 * 86400.0);
  /* The J1950 epoch maps to the start of UNIX time's own predecessor era. */
  DP_CHECK (wfm_j1950_to_unix_sec (0.0) == -631152000.0);
  DP_CHECK (wfm_unix_to_j1950_sec (0.0) == 631152000.0);
}

/* 2026-08-05T04:15:30Z: UNIX 1785903330, J1950 that plus the offset. */
static void
test_round_trip (void)
{
  const double unix_sec = 1785903330.0;
  const double j1950    = unix_sec + WFM_J1950_UNIX_OFFSET_SEC;

  DP_CHECK (wfm_unix_to_j1950_sec (unix_sec) == j1950);
  DP_CHECK (wfm_j1950_to_unix_sec (j1950) == unix_sec);
  /* Exactly reversible at second granularity for present-day instants. */
  DP_CHECK (wfm_j1950_to_unix_sec (wfm_unix_to_j1950_sec (unix_sec))
            == unix_sec);
}

/* The case that makes this more than an offset. doppler's own BLUE writer
   zeroes the timecode field, so "0" is what an unset capture time looks like
   -- and 0 J1950 is a perfectly valid-looking 1950-01-01. Converting it
   through would date every doppler-written capture to 1950, confidently. */
static void
test_zero_is_unset_not_1950 (void)
{
  DP_CHECK (!wfm_timecode_is_set (WFM_TIMECODE_UNSET));
  DP_CHECK (!wfm_timecode_is_set (0.0));
  DP_CHECK (wfm_timecode_is_set (1.0));
  DP_CHECK (wfm_timecode_is_set (WFM_J1950_UNIX_OFFSET_SEC));

  uint64_t ns = 12345u;
  DP_CHECK (wfm_j1950_to_unix_ns (0.0, &ns) == -1);
  DP_CHECK (ns == 12345u); /* untouched on failure */
}

/* A 1950s capture is real BLUE data. It cannot be expressed as UNIX
   nanoseconds in a uint64_t, so the conversion must refuse rather than wrap
   into a plausible far-future timestamp. */
static void
test_pre_1970_refuses_rather_than_wrapping (void)
{
  uint64_t ns = 999u;
  /* 1955-ish: set, but negative once shifted to the UNIX epoch. */
  DP_CHECK (wfm_j1950_to_unix_ns (5.0 * 365.0 * 86400.0, &ns) == -1);
  DP_CHECK (ns == 999u);
  /* One second before the UNIX epoch is still a refusal, not a wrap. */
  DP_CHECK (wfm_j1950_to_unix_ns (WFM_J1950_UNIX_OFFSET_SEC - 1.0, &ns) == -1);
  DP_CHECK (ns == 999u);
  /* The UNIX epoch itself is representable. */
  DP_CHECK (wfm_j1950_to_unix_ns (WFM_J1950_UNIX_OFFSET_SEC, &ns) == 0);
  DP_CHECK (ns == 0u);
}

static void
test_ns_conversion (void)
{
  uint64_t ns = 0u;
  /* 2026-08-05T04:15:30Z */
  const double j1950 = 1785903330.0 + WFM_J1950_UNIX_OFFSET_SEC;
  DP_CHECK (wfm_j1950_to_unix_ns (j1950, &ns) == 0);
  DP_CHECK (ns == UINT64_C (1785903330000000000));

  /* Half a second resolves; see the header's note on why the low digits of
     a BLUE-derived nanosecond value are padding rather than measurement. */
  DP_CHECK (wfm_j1950_to_unix_ns (j1950 + 0.5, &ns) == 0);
  DP_CHECK (ns == UINT64_C (1785903330500000000));

  DP_CHECK (wfm_j1950_to_unix_ns (j1950, NULL) == -1);
}

int
main (void)
{
  test_offset_is_the_documented_constant ();
  test_round_trip ();
  test_zero_is_unset_not_1950 ();
  test_pre_1970_refuses_rather_than_wrapping ();
  test_ns_conversion ();

  DP_TEST_END ("test_wfm_time");
}
