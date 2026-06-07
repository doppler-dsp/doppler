/**
 * @file wfm_writer.h
 * @brief Output containers for generated IQ: raw / csv / BLUE-1000 + SigMF meta.
 *
 * A streaming writer over a FILE* that serialises cf32 blocks into one of three
 * on-disk containers, in the chosen wire sample type and byte order. The fourth
 * file-type, SigMF, writes its samples as `raw` (into `<base>.sigmf-data`) and
 * pairs with a sidecar `<base>.sigmf-meta` JSON emitted by wfm_sigmf_meta_json().
 *
 * Axes (orthogonal to the container):
 *   - sample_type (wavegen order): 0 cf32, 1 cf64, 2 ci32, 3 ci16, 4 ci8.
 *     Integer types quantise full-scale ±1.0 (ci32 2^31-1, ci16 32767, ci8 127).
 *   - endian: 0 little, 1 big (csv is text, so endian is ignored there).
 *
 * @code
 * wfm_writer_t *w = wfm_writer_open(fp, WFM_FT_BLUE, 3, 0, 1e6, 2.4e9, 4096);
 * wfm_writer_write(w, iq, 4096);
 * wfm_writer_close(w);   // patches the BLUE data_size from the actual count
 * @endcode
 */
#ifndef WFM_WRITER_H
#define WFM_WRITER_H

#include <stdio.h>

#include "clib_common.h"
#include "wfmgen/wfm_compose.h" /* wfm_segment_t for SigMF annotations */

#ifdef __cplusplus
extern "C" {
#endif

/** Output container. */
typedef enum {
    WFM_FT_RAW = 0,  /**< interleaved I/Q, no header. */
    WFM_FT_CSV = 1,  /**< text, one complex sample per line. */
    WFM_FT_BLUE = 2, /**< X-Midas/REDHAWK BLUE type-1000 (512-byte header). */
    WFM_FT_SIGMF = 3 /**< samples as raw; metadata via wfm_sigmf_meta_json(). */
} wfm_filetype_t;

/** Opaque writer. */
typedef struct wfm_writer wfm_writer_t;

/**
 * @brief Open a writer on an already-open stream.
 * @param fp            destination (binary mode for raw/blue; text-safe for csv).
 * @param ft            container; SIGMF is treated as RAW here.
 * @param sample_type   wire type (wavegen order); see file header.
 * @param endian        0 little, 1 big (ignored for csv).
 * @param fs            sample rate (Hz) — BLUE xdelta = 1/fs.
 * @param fc            center frequency (Hz) — reserved (BLUE/raw ignore it).
 * @param total_samples expected complex-sample count for the BLUE header
 *                      (0 if unknown; close() patches the actual count when fp
 *                      is seekable).
 * @return Writer handle, or NULL on bad args / allocation. BLUE writes its
 *         512-byte header here.
 */
wfm_writer_t *wfm_writer_open(FILE *fp, wfm_filetype_t ft, int sample_type,
                             int endian, double fs, double fc,
                             size_t total_samples);

/**
 * @brief Convert and write `n` complex samples.
 * @return Number of complex samples written (== n on success, else short).
 */
size_t wfm_writer_write(wfm_writer_t *w, const float _Complex *iq, size_t n);

/**
 * @brief Flush, patch the BLUE data_size from the actual count (if seekable),
 *        and free the writer (does not close the FILE*).
 * @return 0 on success, non-zero on a write/seek error.
 */
int wfm_writer_close(wfm_writer_t *w);

/**
 * @brief Write a complete 512-byte BLUE/Platinum type-1000 Header Control Block.
 *
 * Used for the `blue` container — both attached (the writer calls this with
 * `data_start = 512`, `detached = 0`, then streams the data after it) and
 * detached (the caller writes the data to a separate `.det` file and this HCB to
 * a `.hdr` file with `data_start = 0`, `detached = 1`). Every standard field is
 * written; the header byte order follows `endian`.
 *
 * @param fp            destination (binary).
 * @param sample_type   wire type (wavegen order) → BLUE format char C{B,I,L,F,D}.
 * @param endian        0 little (`EEEI`) / 1 big (`IEEE`).
 * @param fs            sample rate (Hz) → `xdelta = 1/fs`.
 * @param fc            reserved (no standard type-1000 field).
 * @param data_start    `data_start` field: 512 attached, 0 detached.
 * @param total_samples complex-sample count → `data_size`.
 * @param detached      non-zero sets the HCB `detached` flag.
 * @return 0 on success, non-zero on a write error.
 */
int wfm_blue_write_hcb(FILE *fp, int sample_type, int endian, double fs,
                       double fc, double data_start, size_t total_samples,
                       int detached);

/**
 * @brief Build a SigMF `.sigmf-meta` JSON document for a generated capture.
 *
 * `global` carries core:datatype (from sample_type+endian, e.g. "ci16_le"),
 * core:sample_rate, core:version "1.0.0", and a wfmgen description/author.
 * `captures` is a single capture at sample 0 / frequency `fc`. `annotations`
 * has one entry per composer segment — sample span, frequency edges
 * (fc + freq ± bandwidth/2, bandwidth ≈ fs/sps for symbol/chip types), a
 * core:label of the waveform type, and custom `wfmgen:*` parameters.
 *
 * @return malloc'd JSON string (caller frees), or NULL on allocation failure.
 */
char *wfm_sigmf_meta_json(int sample_type, int endian, double fs, double fc,
                          const wfm_segment_t *segs, size_t n_segs);

#ifdef __cplusplus
}
#endif

#endif /* WFM_WRITER_H */
