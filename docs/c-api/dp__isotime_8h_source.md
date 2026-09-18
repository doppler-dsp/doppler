

# File dp\_isotime.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_isotime.h**](dp__isotime_8h.md)

[Go to the documentation of this file](dp__isotime_8h.md)


```C++

#ifndef DP_ISOTIME_H
#define DP_ISOTIME_H

/* clock_gettime is POSIX, and the feature-test macro that
   exposes it is on the COMPILE LINE -- see the top-level CMakeLists.txt,
   which puts it on the exported target too, so a downstream inherits it.
   This header used to raise _POSIX_C_SOURCE itself, which is a no-op for any
   translation unit that reached libc first (doppler#986); the comment on it
   also claimed doppler "builds as strict c99", and it does not --
   CMAKE_C_EXTENSIONS defaults ON, so the dialect is gnu99. */

#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <time.h>

/* Windows has no `clock_gettime`, and its replacement lives in <time.h> --
   so this header does NOT pull in <windows.h>. That matters for a public
   header: windows.h is enormous and drags macros (min/max, near/far) into
   every downstream translation unit that includes doppler.
 *
 * C11's `timespec_get(&ts, TIME_UTC)` is the standard spelling of "wall
 * clock with nanoseconds" and the UCRT provides it. POSIX keeps
 * clock_gettime, which is the one with a documented clock ID rather than an
 * implementation-defined base. (Breaking the instant into fields needs no
 * platform call at all: see dp_isotime_utc_tm_.) */
#ifdef _WIN32
static inline int
dp_isotime_wall (struct timespec *ts)
{
  return timespec_get (ts, TIME_UTC) == TIME_UTC ? 0 : -1;
}
#else
static inline int
dp_isotime_wall (struct timespec *ts)
{
  return clock_gettime (CLOCK_REALTIME, ts);
}
#endif

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
  dp_isotime_utc_tm_ (int64_t sec, struct tm *out)
  {
    /* floor division: 86400 * days + rem with rem in [0, 86399], also for
       sec < 0, which C's truncating `/` would get wrong by one day. */
    int64_t days = sec / 86400;
    int64_t rem  = sec % 86400;
    if (rem < 0)
      {
        rem += 86400;
        days -= 1;
      }

    int64_t        z   = days + 719468;
    const int64_t  era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned) (z - era * 146097);
    const unsigned yoe
        = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp  = (5 * doy + 2) / 153;
    const unsigned d   = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m   = mp < 10 ? mp + 3 : mp - 9;
    const int64_t  y   = (int64_t) yoe + era * 400 + (m <= 2);

    if (y - 1900 > INT_MAX || y - 1900 < INT_MIN)
      return -1;

    static const int cum[12]
        = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    /* A remainder of zero is sign-agnostic in C, so this is the Gregorian
       rule for negative years too. */
    const int leap = (y % 4 == 0) && (y % 100 != 0 || y % 400 == 0);

    memset (out, 0, sizeof *out);
    out->tm_year  = (int) (y - 1900);
    out->tm_mon   = (int) m - 1;
    out->tm_mday  = (int) d;
    out->tm_hour  = (int) (rem / 3600);
    out->tm_min   = (int) (rem % 3600 / 60);
    out->tm_sec   = (int) (rem % 60);
    out->tm_yday  = cum[m - 1] + (int) d - 1 + (m > 2 && leap);
    out->tm_wday  = (int) (((days % 7) + 7 + 4) % 7); /* 1970-01-01: Thu */
    out->tm_isdst = 0;
    return 0;
  }

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

    struct tm tm_utc;
    if (dp_isotime_utc_tm_ (sec, &tm_utc) != 0)
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
    if (dp_isotime_wall (&ts) != 0)
      return -1;
    return dp_isotime_format (buf, cap, (int64_t) ts.tv_sec,
                              (uint32_t) ts.tv_nsec, frac);
  }


  /* ── parsing ───────────────────────────────────────────────────────────
   *
   * The inverse of the formatter above, in this file for the reason the two
   * spellings share one formatter: a parser elsewhere is a second opinion
   * about what this library's own timestamps mean.
   */

  static inline int
  dp_isotime_digits_ (const char **p, int n, int *out)
  {
    int v = 0;
    for (int i = 0; i < n; i++)
      {
        char c = (*p)[i];
        if (c < '0' || c > '9')
          return -1;
        v = v * 10 + (c - '0');
      }
    *p += n;
    *out = v;
    return 0;
  }

  static inline int64_t
  dp_isotime_days_from_civil_ (int64_t y, unsigned m, unsigned d)
  {
    y -= (m <= 2);
    const int64_t  era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy
        = (153u * (m + (m > 2u ? (unsigned)-3 : 9u)) + 2u) / 5u + d - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + (int64_t)doe - 719468;
  }

  static inline int
  dp_isotime_parse (const char *s, int64_t *sec, uint32_t *nsec)
  {
    if (!s || !sec || !nsec)
      return -1;
    const char *p = s;
    int         Y, Mo, D, h, mi, se;
    /* The shortest legal input is the basic form with no fraction,
       `YYYYMMDDThhmmssZ` -- 16 characters. Checked BEFORE s[4] is read,
       because a shorter string makes that read run off the end of the
       caller's buffer rather than merely reaching a NUL: measured by ASan on
       `dp_isotime_parse ("")`, which is one of this parser's own reject
       cases. */
    for (int i = 0; i < 16; i++)
      if (s[i] == '\0')
        return -1;
    /* One format, read two ways: the separators are present or they are not,
       and s[4] is where they first differ. Mixed spellings are rejected by
       the very checks that skip the separators. */
    const int ext = (s[4] == '-');
    if (dp_isotime_digits_ (&p, 4, &Y) != 0)
      return -1;
    if (ext && *p++ != '-')
      return -1;
    if (dp_isotime_digits_ (&p, 2, &Mo) != 0)
      return -1;
    if (ext && *p++ != '-')
      return -1;
    if (dp_isotime_digits_ (&p, 2, &D) != 0)
      return -1;
    if (*p != 'T' && *p != 't' && *p != ' ')
      return -1;
    p++;
    if (dp_isotime_digits_ (&p, 2, &h) != 0)
      return -1;
    if (ext && *p++ != ':')
      return -1;
    if (dp_isotime_digits_ (&p, 2, &mi) != 0)
      return -1;
    if (ext && *p++ != ':')
      return -1;
    if (dp_isotime_digits_ (&p, 2, &se) != 0)
      return -1;
    /* A leap second is spelled :60 and is a real instant on the wire; every
       other field is range-checked so a transposed one cannot slide through
       as a plausible date. */
    if (Mo < 1 || Mo > 12 || D < 1 || D > 31 || h > 23 || mi > 59 || se > 60)
      return -1;

    uint32_t frac = 0;
    if (*p == '.' || *p == ',')
      {
        p++;
        int digits = 0;
        if (*p < '0' || *p > '9')
          return -1;
        /* Scaled to nanoseconds as it is read, so `.5` is 500000000 and not
           5: the field is a FRACTION, not an integer count of anything. */
        for (; *p >= '0' && *p <= '9'; p++, digits++)
          if (digits < 9)
            frac = frac * 10u + (uint32_t)(*p - '0');
        for (int i = digits; i < 9; i++)
          frac *= 10u;
      }

    int64_t offset = 0;
    if (*p == 'Z' || *p == 'z')
      p++;
    else if (*p == '+' || *p == '-')
      {
        const int neg = (*p == '-');
        p++;
        int oh, om = 0;
        if (dp_isotime_digits_ (&p, 2, &oh) != 0)
          return -1;
        if (*p == ':')
          p++;
        if (*p >= '0' && *p <= '9' && dp_isotime_digits_ (&p, 2, &om) != 0)
          return -1;
        if (oh > 23 || om > 59)
          return -1;
        offset = (int64_t)oh * 3600 + (int64_t)om * 60;
        if (neg)
          offset = -offset;
      }
    else
      return -1; /* no zone: see the note above -- refused, never assumed */
    if (*p != '\0')
      return -1;

    const int64_t days
        = dp_isotime_days_from_civil_ ((int64_t)Y, (unsigned)Mo, (unsigned)D);
    *sec  = days * 86400 + (int64_t)h * 3600 + (int64_t)mi * 60 + se - offset;
    *nsec = frac;
    return 0;
  }

#ifdef __cplusplus
}
#endif

#endif /* DP_ISOTIME_H */
```


