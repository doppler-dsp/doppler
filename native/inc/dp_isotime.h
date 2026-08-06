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

#ifdef __cplusplus
}
#endif

#endif /* DP_ISOTIME_H */
