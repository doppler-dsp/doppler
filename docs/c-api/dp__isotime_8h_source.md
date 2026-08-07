

# File dp\_isotime.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_isotime.h**](dp__isotime_8h.md)

[Go to the documentation of this file](dp__isotime_8h.md)


```C++

#ifndef DP_ISOTIME_H
#define DP_ISOTIME_H

/* clock_gettime + gmtime_r are POSIX, and doppler builds as strict c99. */
#if !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 200809L
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define DP_ISOTIME_MAX 32

#define DP_ISOTIME_SEC 0u
#define DP_ISOTIME_MSEC 3u
#define DP_ISOTIME_USEC 6u
#define DP_ISOTIME_NSEC 9u

#define DP_ISOTIME_BASIC 0
#define DP_ISOTIME_EXTENDED 1

  static inline int
  dp_isotime_format_as (char *buf, size_t cap, int64_t sec, uint32_t nsec,
                        unsigned frac, int style)
  {
    if (!buf || nsec > 999999999u)
      return -1;
    if (frac != DP_ISOTIME_SEC && frac != DP_ISOTIME_MSEC
        && frac != DP_ISOTIME_USEC && frac != DP_ISOTIME_NSEC)
      return -1;
    if (style != DP_ISOTIME_BASIC && style != DP_ISOTIME_EXTENDED)
      return -1;

    time_t    t = (time_t) sec;
    struct tm tm_utc;
    if (!gmtime_r (&t, &tm_utc))
      return -1;

    /* Basic "YYYYMMDDThhmmss" is 15 chars and carries no colons; extended
       "YYYY-MM-DDThh:mm:ss" is 19 and does. Only the separators differ, so
       the calendar break-down above is done once for both. */
    char date[24];
    if (strftime (date, sizeof date,
                  style == DP_ISOTIME_EXTENDED ? "%Y-%m-%dT%H:%M:%S"
                                               : "%Y%m%dT%H%M%S",
                  &tm_utc)
        == 0)
      return -1;

    if (frac == DP_ISOTIME_SEC)
      {
        int n = snprintf (buf, cap, "%sZ", date);
        return (n < 0 || (size_t) n >= cap) ? -1 : n;
      }

    /* TRUNCATE. `nsec / 10^(9-frac)` is integer division, so .999888777
       becomes .999 at millisecond precision rather than carrying to 1.000
       and stepping the seconds field. Never use a rounding conversion. */
    uint32_t scale = 1u;
    for (unsigned i = frac; i < DP_ISOTIME_NSEC; i++)
      scale *= 10u;

    int n = snprintf (buf, cap, "%s.%0*luZ", date, (int) frac,
                      (unsigned long) (nsec / scale));
    return (n < 0 || (size_t) n >= cap) ? -1 : n;
  }

  static inline int
  dp_isotime_format (char *buf, size_t cap, int64_t sec, uint32_t nsec,
                     unsigned frac)
  {
    return dp_isotime_format_as (buf, cap, sec, nsec, frac,
                                 DP_ISOTIME_BASIC);
  }

  static inline int
  dp_isotime_now (char *buf, size_t cap, unsigned frac)
  {
    struct timespec ts;
    if (clock_gettime (CLOCK_REALTIME, &ts) != 0)
      return -1;
    return dp_isotime_format (buf, cap, (int64_t) ts.tv_sec,
                              (uint32_t) ts.tv_nsec, frac);
  }

#ifdef __cplusplus
}
#endif

#endif /* DP_ISOTIME_H */
```


