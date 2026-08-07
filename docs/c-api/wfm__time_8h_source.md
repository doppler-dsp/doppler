

# File wfm\_time.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_time.h**](wfm__time_8h.md)

[Go to the documentation of this file](wfm__time_8h.md)


```C++
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

#define WFM_J1950_UNIX_OFFSET_SEC 631152000.0

#define WFM_TIMECODE_UNSET 0.0

static inline int
wfm_timecode_is_set (double timecode)
{
  return timecode != WFM_TIMECODE_UNSET;
}

static inline double
wfm_j1950_to_unix_sec (double t_j1950)
{
  return t_j1950 - WFM_J1950_UNIX_OFFSET_SEC;
}

static inline double
wfm_unix_to_j1950_sec (double t_unix)
{
  return t_unix + WFM_J1950_UNIX_OFFSET_SEC;
}

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
```


