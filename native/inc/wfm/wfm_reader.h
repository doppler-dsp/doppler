/**
 * @file wfm_reader.h
 * @brief Input containers for generated IQ — the dual of wfm_writer.
 *
 * Reads back what wfm_writer wrote: raw interleaved I/Q, CSV, BLUE type-1000
 * (attached or detached), and SigMF. The container is **auto-detected** from
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
 * wfm_reader_t *r = wfm_reader_open("cap.sigmf-data", 0, 0);
 * wfm_reader_info_t info;
 * wfm_reader_info(r, &info);                 // info.fs, info.sample_type, ...
 * float _Complex buf[4096];
 * size_t n;
 * while ((n = wfm_reader_read(r, buf, 4096)) > 0)
 *   consume(buf, n);
 * wfm_reader_close(r);
 * @endcode
 */
#ifndef DP_WFM_READER_H
#define DP_WFM_READER_H

#include <complex.h>
#include <stddef.h>

#include "wfm/wfm_writer.h" /* wfm_filetype_t */

#ifdef __cplusplus
extern "C"
{
#endif

  /** Opaque reader handle. */
  typedef struct wfm_reader wfm_reader_t;

  /** Resolved metadata for an open capture. Fields the container does not
   *  carry are 0 (`fs`/`fc` for raw/CSV, `num_samples` for a stream). */
  typedef struct
  {
    int    file_type;   /**< detected wfm_filetype_t. */
    int    sample_type; /**< 0 cf32, 1 cf64, 2 ci32, 3 ci16, 4 ci8. */
    int    endian;      /**< 0 little, 1 big. */
    double fs;          /**< sample rate (Hz); 0 if unknown. */
    double fc;          /**< centre frequency (Hz); 0 if unknown. */
    size_t num_samples; /**< total complex samples; 0 if unknown. */
  } wfm_reader_info_t;

  /**
   * @brief Open a capture, auto-detecting its container.
   *
   * @param path           file to read. A BLUE `.det` or SigMF `.sigmf-data`
   *                       data file resolves its `.hdr` / `.sigmf-meta`
   *                       sidecar automatically.
   * @param hint_sample_type  sample type (0..4) for headerless raw/CSV; ignored
   *                       once BLUE/SigMF metadata is parsed.
   * @param hint_endian    byte order (0 le, 1 be) for headerless raw.
   * @return a reader, or NULL on open/parse failure.
   */
  wfm_reader_t *wfm_reader_open (const char *path, int hint_sample_type,
                                 int hint_endian);

  /** @brief Copy the resolved capture metadata into @p info. */
  void wfm_reader_info (const wfm_reader_t *r, wfm_reader_info_t *info);

  /**
   * @brief Read up to @p max complex samples into @p out (unit-scale
   * `float _Complex`), converting from the wire type. Returns the count read;
   * 0 at end of file.
   */
  size_t wfm_reader_read (wfm_reader_t *r, float _Complex *out, size_t max);

  /** @brief Close the file and free the reader. */
  void wfm_reader_close (wfm_reader_t *r);

#ifdef __cplusplus
}
#endif

#endif /* DP_WFM_READER_H */
