/**
 * @file wfm_reader_core.h
 * @brief Input containers for generated IQ — the dual of wfm_writer.
 *
 * Reads back what wfm_writer wrote: raw interleaved I/Q, CSV, BLUE type-1000
 * (attached or detached, `format` mode `S` or `C`), and SigMF. A BLUE file in
 * any other mode is rejected at open — see ::wfm_mode_t.
 * The container is **auto-detected** from
 * the file (BLUE magic / `.sigmf-meta` sidecar / `.csv` extension), and
 * self-describing containers (BLUE, SigMF) recover the sample type, byte order,
 * sample rate and centre frequency from their metadata. Headerless containers
 * (raw, CSV) take the sample type / byte order as hints.
 *
 * Samples come out as `float _Complex` at unit scale: float wire types are
 * reinterpreted, integer wire types are rescaled by their full-scale (the exact
 * inverse of the writer's quantiser).
 *
 * @code
 * wfm_reader_state_t *r = wfm_reader_create("cap.sigmf-data", 0, 0);
 * wfm_reader_info_t info;
 * wfm_reader_info(r, &info);                 // info.fs, info.sample_type, ...
 * float _Complex buf[4096];
 * size_t n;
 * while ((n = wfm_reader_read(r, buf, 4096)) > 0)
 *   consume(buf, n);
 * wfm_reader_destroy(r);
 * @endcode
 */
#ifndef DP_WFM_READER_H
#define DP_WFM_READER_H

#include <complex.h>
#include <stddef.h>

#include "wfm/wfm_keywords.h" /* wfm_keyword_t */
#include "wfm_writer/wfm_writer_core.h"   /* wfm_filetype_t */

#ifdef __cplusplus
extern "C"
{
#endif

  /** Opaque reader handle. */
  /** Opaque reader state; the layout is private to wfm_reader_core.c. */
  typedef struct wfm_reader_state wfm_reader_state_t;


  /** Components per sample — the BLUE `format` field's *mode* designator
   *  (HCB byte 52). Only these two are supported; every other Midas mode
   *  (V/Q/M/T/…, 3..10 components) is rejected at open rather than
   *  misinterpreted as interleaved I/Q. Non-BLUE containers are complex. */
  typedef enum
  {
    WFM_MODE_COMPLEX = 0, /**< 'C' — interleaved I/Q, two components. */
    WFM_MODE_SCALAR = 1   /**< 'S' — real, one component (Q read as 0). */
  } wfm_mode_t;

  /** Resolved metadata for an open capture. Fields the container does not
   *  carry are 0 (`fs`/`fc` for raw/CSV, `num_samples` for a stream). */
  typedef struct
  {
    int    file_type;   /**< detected wfm_filetype_t. */
    int    sample_type; /**< 0 cf32, 1 cf64, 2 ci32, 3 ci16, 4 ci8. */
    int    mode;        /**< wfm_mode_t: 0 complex, 1 scalar (BLUE 'S'). */
    int    endian;      /**< 0 little, 1 big. */
    double fs;          /**< sample rate (Hz); 0 if unknown. */
    double fc;          /**< centre frequency (Hz); 0 if unknown. */
    size_t num_samples; /**< total complex samples; 0 if unknown. */
  } wfm_reader_info_t;

  /**
   * @brief Open a capture, auto-detecting its container.
   *
   * @param path           file to read. For a DETACHED BLUE capture this is
   *                       normally the HEADER file -- `<base>.tmp` or
   *                       `<base>.prm` per BLUE 3.1.1.4 (this library's own
   *                       writer emits `<base>.hdr`) -- whose HCB `detached`
   *                       field points at the collocated `<base>.det`
   *                       payload; the extension does not decide, `detached`
   *                       does. Passing the `<base>.det` directly also works
   *                       (its header sibling is resolved). A SigMF
   *                       `.sigmf-data` file resolves its `.sigmf-meta`
   *                       sidecar the same way.
   * @param sample_type    sample type (0..4) used as a HINT for headerless
   *                       raw/CSV; ignored once BLUE/SigMF metadata is parsed,
   *                       which carries its own.
   * @param endian         byte order (0 le, 1 be), likewise a hint for
   *                       headerless raw.
   * @return a reader, or NULL on open/parse failure.
   */
wfm_reader_state_t *wfm_reader_create(const char *path, int sample_type, int endian);

  /** @brief Copy the resolved capture metadata into @p info. */
  void wfm_reader_info (const wfm_reader_state_t *r, wfm_reader_info_t *info);

  /**
   * @brief Read up to @p n complex samples into @p out (unit-scale
   * `float _Complex`), converting from the wire type. Returns the count read;
   * 0 at end of file, and never more than the container's declared payload.
   */
size_t wfm_reader_read(wfm_reader_state_t *state, size_t n, float complex *out);

  /** @brief Upper bound on one read()'s output, or 0 for "unbounded".
   *
   *  A reader streams, so it declares no bound and the generated binding sizes
   *  its buffer from the caller's request instead of pre-allocating the whole
   *  capture at construction. */
size_t wfm_reader_read_max_out(wfm_reader_state_t *state);

  /**
   * @brief Number of extended-header keywords recovered from the capture.
   *
   * BLUE only, and 0 unless the file carries an extended header. Keywords of
   * a type this library cannot decode are skipped during the walk (BLUE
   * §3.3.1) and are not counted; a truncated or malformed keyword region
   * yields whatever decoded cleanly before it, since metadata must never cost
   * you the samples. For a detached capture the keywords come from the HEADER
   * file, not the `.det`.
   */
  size_t wfm_reader_num_keywords (const wfm_reader_state_t *r);

  /**
   * @brief The @p i'th keyword in file order, or NULL if @p i is out of range.
   *
   * The returned pointer (and its `value` buffer) is owned by the reader and
   * is freed by wfm_reader_destroy().
   */
  const wfm_keyword_t *wfm_reader_keyword (const wfm_reader_state_t *r, size_t i);

  /**
   * @brief The first keyword whose tag equals @p tag, or NULL if absent.
   *
   * Tags are not required to be unique; this returns the earliest match.
   */
  const wfm_keyword_t *wfm_reader_find_keyword (const wfm_reader_state_t *r,
                                                const char        *tag);

  /**
   * @brief Rewind to the first sample of the capture.
   *
   * Seeks back to where the payload starts — 512 bytes into an attached BLUE
   * file, byte 0 of a `.det` or a raw/SigMF payload — and restores the
   * remaining-sample count, so the capture reads again from the top. The
   * container metadata and decoded keywords are unaffected: they came from the
   * header and do not change.
   */
void wfm_reader_reset(wfm_reader_state_t *state);

  /** @brief Close the file, free the reader and its decoded keywords. */
void wfm_reader_destroy(wfm_reader_state_t *state);

int wfm_reader_get_file_type(const wfm_reader_state_t *state);
int wfm_reader_get_sample_type(const wfm_reader_state_t *state);
int wfm_reader_get_mode(const wfm_reader_state_t *state);
int wfm_reader_get_endian(const wfm_reader_state_t *state);
double wfm_reader_get_fs(const wfm_reader_state_t *state);
double wfm_reader_get_fc(const wfm_reader_state_t *state);
size_t wfm_reader_get_num_samples(const wfm_reader_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* DP_WFM_READER_H */
