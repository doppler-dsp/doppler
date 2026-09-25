/*
 * wfm_time.h — the one BLUE timecode <-> UNIX epoch conversion.
 *
 * A BLUE type-1000 header carries its capture time at byte 56 as a `double`
 * of seconds since the **J1950** epoch (1950-01-01T00:00:00Z), which is
 * 631152000 seconds before the UNIX epoch. Nothing in doppler converted it:
 * `wfm_reader_core.c` reads the field through and hands back the raw double,
 * so a caller wanting an absolute time had to know the offset itself — which
 * is how a magic 631152000 ends up pasted into three call sites.
 *
 * This is the feed into `dp_sample_clock_track()`. The sample clock already
 * owns the time base and the `t0 + n/fs` arithmetic (timing/timing_core.h);
 * all that was missing was a way to get a capture's own epoch into it. Only
 * the wfm family knows what J1950 is — telemetry consumes UNIX epochs and
 * must never learn.
 *
 * Header-only, like dp_crc16.h: wfm_reader and wfm_writer are separate
 * components and neither should grow a link-line dependency for an offset.
 */
#ifndef WFM_TIME_H
#define WFM_TIME_H

#include <stdint.h>

/**
 * Seconds from the J1950 epoch to the UNIX epoch.
 *
 * 20 years, of which 1952/56/60/64/68 were leap: `20*365 + 5 = 7305` days,
 * `7305 * 86400 = 631152000`.
 */
#define WFM_J1950_UNIX_OFFSET_SEC 631152000.0

/**
 * The value a BLUE header carries when nothing set a capture time.
 *
 * **A zero timecode is "unset", not 1950-01-01.** doppler's own writer
 * leaves the field zero (`wfm_writer_core.c` zeroes bytes 54-96), and so do
 * many producers, so converting a zero straight through would date every
 * such capture to 1950 — confidently, and wrongly. Same ambiguity
 * `wfm_fc_source_t` exists to resolve for centre frequency: ask whether it
 * was set before trusting what it says.
 */
#define WFM_TIMECODE_UNSET 0.0

/**
 * Nonzero if @p timecode carries a real capture time.
 *
 * @param timecode Raw BLUE header timecode, J1950 seconds.
 */
static inline int
wfm_timecode_is_set (double timecode)
{
  return timecode != WFM_TIMECODE_UNSET;
}

/**
 * Convert a BLUE timecode to seconds since the UNIX epoch.
 *
 * No range check: the result is simply shifted, and a pre-1970 capture is a
 * legitimate negative. Guard with ::wfm_timecode_is_set first if a zero
 * would be meaningless to the caller.
 *
 * @param t_j1950 Seconds since 1950-01-01T00:00:00Z.
 * @return Seconds since 1970-01-01T00:00:00Z.
 */
static inline double
wfm_j1950_to_unix_sec (double t_j1950)
{
  return t_j1950 - WFM_J1950_UNIX_OFFSET_SEC;
}

/**
 * Convert seconds since the UNIX epoch to a BLUE timecode.
 *
 * @param t_unix Seconds since 1970-01-01T00:00:00Z.
 * @return Seconds since 1950-01-01T00:00:00Z.
 */
static inline double
wfm_unix_to_j1950_sec (double t_unix)
{
  return t_unix + WFM_J1950_UNIX_OFFSET_SEC;
}

/**
 * Convert a BLUE timecode to UNIX nanoseconds, the form
 * `dp_sample_clock_track()` takes.
 *
 * Fails rather than wrapping for the two cases a `uint64_t` cannot express:
 * an unset timecode, and any instant before 1970. A 1950s capture is real
 * BLUE data, so this returning nonzero is a normal answer, not an error to
 * paper over — the caller has an epoch it simply cannot hand to the sample
 * clock.
 *
 * @note **Nanosecond precision is not available from BLUE.** The timecode is
 * a `double` of seconds, so near the present its own resolution is about
 * half a microsecond (`2^-52 * 2^31`). The nanosecond result is exact
 * arithmetic on an input that is not itself nanosecond-accurate; treat the
 * bottom three digits as padding.
 *
 * @param t_j1950 Seconds since 1950-01-01T00:00:00Z.
 * @param out_ns  Receives nanoseconds since the UNIX epoch; untouched on
 *                failure.
 * @return 0 on success, -1 if the timecode is unset or predates 1970.
 */
static inline int
wfm_j1950_to_unix_ns (double t_j1950, uint64_t *out_ns)
{
  if (!out_ns || !wfm_timecode_is_set (t_j1950))
    return -1;
  double secs = wfm_j1950_to_unix_sec (t_j1950);
  if (secs < 0.0)
    return -1;
  *out_ns = (uint64_t) (secs * 1e9);
  return 0;
}

#endif /* WFM_TIME_H */
