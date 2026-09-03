/**
 * @file dp_isotime.h
 * @brief ISO 8601 UTC timestamps in both spellings — filename-safe **basic**
 * for names doppler writes, **extended** for the wire formats that mandate
 * it.
 *
 * Extended ISO 8601 (`2026-08-05T04:15:30Z`) is what a human reads, what
 * doppler's CLI logs print, and what SigMF's `core:datetime` requires. It is
 * also illegal in a filename on Windows and FAT, because of the colons, and
 * awkward to quote in a shell. The basic form drops the separators:
 *
 *     YYYYMMDDThhmmss[.fff[fff[fff]]]Z
 *
 * Both come out of ::dp_isotime_format_as, one calendar computation and one
 * truncation rule rendered two ways, because the only thing that differs is
 * whether `strftime` writes the separators. A second formatter would be a
 * second place for the truncation rule below to be got wrong.
 *
 * **The basic format is not defined here.** It is `just-bashit`'s
 * `iso-8601-basic` (`src/just_bashit/datetime.sh`), whose stated contract is
 * "path and file-name-friendly characters only". doppler formats it in C
 * rather than shelling out — a library shipped as a wheel cannot put its
 * file naming behind a runtime `bash` + `date`/`gdate` lookup on `PATH`, and
 * `clock_gettime` hands back the nanoseconds the sub-second field needs
 * anyway. Code cannot be shared between a bash library and a C one, so the
 * agreement is held by the golden vectors in `native/tests/test_dp_isotime.c`
 * rather than asserted.
 *
 * **The fraction truncates; it never rounds.** `.999888777` at millisecond
 * precision is `.999`, matching the shell helper. Rounding would carry
 * `.9996` to `1.000` and step the seconds field, emitting a timestamp one
 * second in the future that disagrees with every name written beside it. The
 * integer division below is what makes that structural rather than a
 * convention someone has to remember.
 *
 * Header-only, like `dp_crc16.h`, so no component grows a link-line
 * dependency for a formatter.
 *
 * @note `CLOCK_REALTIME` steps under NTP. These names are unique and
 * human-readable, **not** a chronological sort key.
 */
#ifndef DP_ISOTIME_H
#define DP_ISOTIME_H

/* clock_gettime + gmtime_r are POSIX, and the feature-test macro that
   exposes them is on the COMPILE LINE -- see the top-level CMakeLists.txt,
   which puts it on the exported target too, so a downstream inherits it.
   This header used to raise _POSIX_C_SOURCE itself, which is a no-op for any
   translation unit that reached libc first (doppler#986); the comment on it
   also claimed doppler "builds as strict c99", and it does not --
   CMAKE_C_EXTENSIONS defaults ON, so the dialect is gnu99. */

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Bytes needed for the longest form (nanoseconds) plus the NUL. */
#define DP_ISOTIME_MAX 32

/** Fractional digits `iso-8601-basic` offers: none, `-m`, `-u`, `-n`. */
#define DP_ISOTIME_SEC 0u
#define DP_ISOTIME_MSEC 3u
#define DP_ISOTIME_USEC 6u
#define DP_ISOTIME_NSEC 9u

/** Separator style: `20260805T041530Z`, safe in a filename. */
#define DP_ISOTIME_BASIC 0
/** Separator style: `2026-08-05T04:15:30Z`, what SigMF and humans want. */
#define DP_ISOTIME_EXTENDED 1

  /**
   * Format one instant as a UTC timestamp in either separator style.
   *
   * @param buf   Destination; receives a NUL-terminated string.
   * @param cap   Size of @p buf; ::DP_ISOTIME_MAX is always enough.
   * @param sec   Seconds since the UNIX epoch (UTC).
   * @param nsec  Nanoseconds within that second, `[0, 999999999]`.
   * @param frac  Fractional digits: 0, 3, 6 or 9 (the ::DP_ISOTIME_MSEC
   *              family). Any other value is rejected.
   * @param style ::DP_ISOTIME_BASIC or ::DP_ISOTIME_EXTENDED.
   * @return Characters written (excluding the NUL), or -1 if @p frac is not
   * one of the four, @p style is neither, @p nsec is out of range, @p buf is
   * too small, or the instant is not representable as a UTC calendar time.
   */
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

  /**
   * Format one instant as a filename-safe basic-format UTC timestamp.
   *
   * The default spelling: this is the one that goes in a name doppler
   * writes. ::dp_isotime_format_as with ::DP_ISOTIME_EXTENDED is for the
   * wire formats that mandate separators.
   *
   * @param buf   Destination; receives a NUL-terminated string.
   * @param cap   Size of @p buf; ::DP_ISOTIME_MAX is always enough.
   * @param sec   Seconds since the UNIX epoch (UTC).
   * @param nsec  Nanoseconds within that second, `[0, 999999999]`.
   * @param frac  Fractional digits, as ::dp_isotime_format_as.
   * @return As ::dp_isotime_format_as.
   */
  static inline int
  dp_isotime_format (char *buf, size_t cap, int64_t sec, uint32_t nsec,
                     unsigned frac)
  {
    return dp_isotime_format_as (buf, cap, sec, nsec, frac,
                                 DP_ISOTIME_BASIC);
  }

  /**
   * Format the current wall clock, the way a capture filename wants it.
   *
   * Reads `CLOCK_REALTIME` directly: no `PATH` lookup, no subprocess, and
   * the nanoseconds arrive already split from the seconds.
   *
   * @param buf  Destination; receives a NUL-terminated string.
   * @param cap  Size of @p buf; ::DP_ISOTIME_MAX is always enough.
   * @param frac Fractional digits, as ::dp_isotime_format. **Prefer
   *             ::DP_ISOTIME_MSEC or finer when the stamp is being used to
   *             keep filenames apart** — two captures written in the same
   *             second collide at seconds resolution.
   * @return As ::dp_isotime_format, or -1 if the clock read fails.
   */
  static inline int
  dp_isotime_now (char *buf, size_t cap, unsigned frac)
  {
    struct timespec ts;
    if (clock_gettime (CLOCK_REALTIME, &ts) != 0)
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

  /** @brief Reads @p n decimal digits, advancing @p p.  0 on success. */
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

  /**
   * @brief Days since 1970-01-01 for a proleptic-Gregorian civil date.
   *
   * Howard Hinnant's `days_from_civil`, which is exact for every date and
   * needs no timezone database.  Written out rather than reached through
   * `timegm`: that function is neither C nor POSIX, and the one thing this
   * parser must never do is consult the ambient `TZ`.
   */
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

  /**
   * @brief Parses an ISO 8601 UTC timestamp into UNIX seconds + nanoseconds.
   *
   * Accepts both spellings this file writes — extended
   * (`2026-08-05T04:15:30.123456Z`) and basic (`20260805T041530Z`) — with an
   * optional fraction of one to nine digits, and either `Z` or an explicit
   * `+hh:mm` / `-hhmm` offset, which is applied.
   *
   * **A timestamp with NO zone is REFUSED.**  It is the one input where
   * guessing costs hours rather than nothing: read as UTC, a local-time
   * stamp dates a capture wrong and looks authoritative doing it.  A caller
   * that cannot parse a start time has ways to say so (the reader reports
   * ::WFM_T0_NONE); a caller holding a wrong one does not.
   *
   * @param s    The timestamp.
   * @param sec  Receives UNIX seconds (may be negative, before 1970).
   * @param nsec Receives the nanoseconds part, 0 when no fraction is given.
   * @return 0 on success, -1 on a NULL argument or any malformed field.
   *
   * @code
   *   int64_t  sec;
   *   uint32_t nsec;
   *   dp_isotime_parse ("1970-01-01T00:00:01Z", &sec, &nsec);  // sec == 1
   * @endcode
   */
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
