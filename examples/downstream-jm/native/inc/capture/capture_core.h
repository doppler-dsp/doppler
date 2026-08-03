/**
 * @file capture_core.h
 * @brief A downstream façade over doppler's `wfm_reader`.
 *
 * This header is the whole C surface of the example. Every function below is
 * glue: it holds doppler's opaque reader handle and forwards to `wfm_reader_*`
 * from `libdoppler.a`. No signal processing is implemented, re-implemented or
 * copied here — that is the point. A downstream project reuses the C core and
 * adds only what is genuinely its own, which in this case is *how the capture
 * is opened* and *whether its metadata can be trusted*.
 *
 * Two constructors share this one state:
 *
 * - `capture_create(path)` — detect everything from the file. Right for a
 *   self-describing capture (BLUE, SigMF); a guess for a headerless one.
 * - `capture_open_raw(path, sample_type, endian, fs, fc)` — the honest
 *   constructor for `raw`/`csv`, where the file carries no sample type, no
 *   sample rate and no centre frequency, so the caller must supply them.
 *
 * They surface in Python as `Capture` and `RawCapture` — one C core, two
 * classes, declared as a just-makeit *view* and generated with no hand-written
 * CPython at all.
 */
#ifndef CAPTURE_CORE_H
#define CAPTURE_CORE_H

#include "clib_common.h"

/* The CaptureSummary record lives in its own header; the field docs derive
   from it across this include (just-makeit gh-724) with no manifest prose. */
#include "capture/capture_summary.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Opaque capture handle.
   *
   * Hand-written (`no_state = "true"` in the manifest) because it wraps
   * doppler's own opaque `wfm_reader_state_t *`, which just-makeit has no way
   * to infer. The layout stays private to `capture_core.c`; callers —
   * including the generated binding — only ever hold the pointer.
   */
  typedef struct capture_state capture_state_t;

  /**
   * @brief Where a capture's `fs`/`fc` came from.
   *
   * Mirrors the `metadata_source` enum in `just-makeit.toml`; the binding
   * decodes it to the matching string. Ordering is load-bearing — the values
   * are indices.
   */
  typedef enum
  {
    CAPTURE_META_NONE     = 0, /**< Headerless file, nothing supplied. */
    CAPTURE_META_FILE     = 1, /**< Read out of the capture itself.    */
    CAPTURE_META_SUPPLIED = 2, /**< Passed to `capture_open_raw`.      */
  } capture_meta_t;

  /* Declare module-level functions here. */

  capture_state_t *capture_create (const char *path);
  void             capture_destroy (capture_state_t *state);
  void             capture_reset (capture_state_t *state);
size_t capture_read_max_out(capture_state_t *state, size_t n);
  size_t capture_read (capture_state_t *state, size_t n, float complex *out,
                       size_t max_out);
  double capture_get_fs (const capture_state_t *state);
  double capture_get_fc (const capture_state_t *state);
  size_t capture_get_num_samples (const capture_state_t *state);
  int    capture_get_metadata_source (const capture_state_t *state);
  capture_summary_t capture_summary (const capture_state_t *state);
  capture_state_t *capture_open_raw (const char *path, int sample_type,
                                     int endian, double fs, double fc);
#ifdef __cplusplus
}
#endif

#endif /* CAPTURE_CORE_H */
