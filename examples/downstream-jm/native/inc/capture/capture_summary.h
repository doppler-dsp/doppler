/**
 * @file capture_summary.h
 * @brief The one-call capture snapshot — in its own header, on purpose.
 *
 * `capture_core.h` includes this, and that include is the whole trick:
 * just-makeit follows it (gh-724) and derives every `CaptureSummary` field's
 * Python docstring straight from the `///<` comments below. The manifest lists
 * the fields; it never restates their prose. Write it once, in C, and it lands
 * on the `.pyi` — the same way doppler's own metric records derive from a
 * shared header.
 */
#ifndef CAPTURE_SUMMARY_H
#define CAPTURE_SUMMARY_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief A capture at a glance: length, and the tuning that was resolved. */
  typedef struct
  {
    size_t num_samples; ///< Samples the reader decoded from the capture.
    double fs_hz;       ///< Sample rate (Hz); 0 if the file never stated it.
    double fc_hz;       ///< Centre frequency (Hz); 0 if the file was silent.
  } capture_summary_t;

#ifdef __cplusplus
}
#endif

#endif /* CAPTURE_SUMMARY_H */
