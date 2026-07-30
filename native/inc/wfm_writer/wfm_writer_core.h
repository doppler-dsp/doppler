/**
 * @file wfm_writer_core.h
 * @brief Output file types for generated IQ: raw / csv / BLUE-1000 + SigMF meta.
 *
 * A streaming writer over a FILE* that serialises cf32 blocks into one of three
 * on-disk file types, in the chosen wire sample type and byte order. The fourth
 * file-type, SigMF, writes its samples as `raw` (into `<base>.sigmf-data`) and
 * pairs with a sidecar `<base>.sigmf-meta` JSON from wfm_sigmf_meta_json().
 *
 * A writer opened by PATH (wfm_writer_create) emits that sidecar itself, at
 * close, so `sigmf` produces a readable pair with no further work — and for
 * that reason it REQUIRES a path ending in `.sigmf-data`, since both halves
 * of a SigMF capture are found by name. A writer opened on a FILE*
 * (wfm_writer_open) has no name to derive the sidecar's from, so the caller
 * owns it — that is the path wfmgen and Composer take, and it is also how
 * they attach their per-segment annotations.
 *
 * Axes (orthogonal to the file type):
 *   - sample_type (wavegen order): 0 cf32, 1 cf64, 2 ci32, 3 ci16, 4 ci8.
 *     Integer types quantise full-scale ±1.0 (ci32 2^31-1, ci16 32767, ci8 127).
 *   - endian: 0 little, 1 big (csv is text, so endian is ignored there).
 *
 * @code
 * wfm_writer_state_t *w = wfm_writer_open(fp, WFM_FT_BLUE, 3, 0, 1e6, 2.4e9, 4096);
 * wfm_writer_write(w, iq, 4096);
 * wfm_writer_close(w);   // patches the BLUE data_size from the actual count
 * @endcode
 */
#ifndef WFM_WRITER_H
#define WFM_WRITER_H

#include <stdbool.h>
#include <stdio.h>

#include "clib_common.h"
#include "wfm/wfm_compose.h" /* wfm_segment_t for SigMF annotations */

#ifdef __cplusplus
extern "C" {
#endif

/** Output file type. */
typedef enum {
    WFM_FT_RAW = 0,  /**< interleaved I/Q, no header. */
    WFM_FT_CSV = 1,  /**< text, one complex sample per line. */
    WFM_FT_BLUE = 2, /**< X-Midas/REDHAWK BLUE type-1000 (512-byte header). */
    WFM_FT_SIGMF = 3 /**< samples as raw; metadata via wfm_sigmf_meta_json(). */
} wfm_filetype_t;

/** Opaque writer. */
typedef struct wfm_writer_state wfm_writer_state_t;


/**
 * @brief Open a writer on an already-open stream.
 * @param fp            destination (binary mode for raw/blue; text-safe for csv).
 * @param ft            file type; SIGMF is treated as RAW here.
 * @param sample_type   wire type (wavegen order); see file header.
 * @param endian        0 little, 1 big (ignored for csv).
 * @param fs            sample rate (Hz) — BLUE xdelta = 1/fs.
 * @param fc            centre frequency (Hz). BLUE records it as a `FREQ`
 *                      keyword — see wfm_writer_create; raw and CSV have
 *                      nowhere to put it and drop it.
 * @param total_samples expected complex-sample count for the BLUE header
 *                      (0 if unknown; close() patches the actual count when fp
 *                      is seekable).
 * @return Writer handle, or NULL on bad args / allocation. BLUE writes its
 *         512-byte header here.
 */
wfm_writer_state_t *wfm_writer_open(FILE *fp, wfm_filetype_t ft, int sample_type,
                             int endian, double fs, double fc,
                             size_t total_samples);

/**
 * @brief Convert and write a block of samples.
 *
 * Takes `complex64` at unit scale and emits it in the writer's wire type.
 * Call as many times as you like; the capture is the concatenation.
 *
 * @return the number of samples that actually landed — equal to what you
 *         passed on success, fewer if the write was short (a full disk, a
 *         quota). A short return is the per-block signal; close() reports the
 *         same failure for the capture as a whole.
 *
 * @code
 * >>> import pathlib, tempfile
 * >>> from doppler.wfm import Composer, Reader, Segment, Writer
 * >>> tmp = tempfile.TemporaryDirectory()
 * >>> p = pathlib.Path(tmp.name) / "capture.blue"
 * >>> x = Composer([Segment("qpsk", sps=8, num_samples=1024)]).compose()
 * >>> with Writer(p, file_type="blue", sample_type="ci16",
 * ...             fs=2.4e6, fc=1.2e9) as w:
 * ...     w.write(x)
 * 1024
 * >>> r = Reader(p)
 * >>> r.fs, r.fc, r.num_samples
 * (2400000.0, 1200000000.0, 1024)
 * >>> r.close()
 * >>> tmp.cleanup()   # directory and contents removed
 * @endcode
 */
size_t wfm_writer_write(wfm_writer_state_t *state, const float complex *x, size_t x_len);

/**
 * @brief Attach a BLUE extended-header keyword (a tag/value pair).
 *
 * Keywords are buffered and written as one block by wfm_writer_close(), after
 * the data — the layout BLUE §3.3 recommends for streaming, since the total
 * data size is not known until the stream ends. `ext_start`/`ext_size` are
 * patched into the HCB at the same time. Call as many times as you like,
 * before or between writes; order is preserved, and duplicate tags are
 * allowed (the format permits them).
 *
 * @param w     an open BLUE writer (any other file type returns an error —
 *              only BLUE has an extended header).
 * @param tag   NUL-terminated tag, 1..255 characters. Upper-case is strongly
 *              preferred: lower-case has limited support across the Midas
 *              baselines.
 * @param type  element type code — `B`/`I`/`L`/`X` (8/16/32/64-bit integer),
 *              `F`/`D` (32/64-bit float), or `A` (ASCII string, variable
 *              length in keyword context). `O`/`P`/`N` are not permitted in
 *              keywords and are rejected.
 * @param value @p count elements in host byte order; for `A`, @p count
 *              characters (no NUL is written or required).
 * @param count element count; must be non-zero.
 * @return 0 on success, non-zero if the file type is not BLUE, the arguments
 *         are invalid, or the buffer could not grow.
 *
 * @code
 * double fc = 1.2345e9;
 * wfm_writer_add_keyword(w, "F_C", 'D', &fc, 1);
 * wfm_writer_add_keyword(w, "COMMENT", 'A', "10 dB pad", 9);
 * wfm_writer_close(w);   // keywords land after the data, HCB patched
 * @endcode
 */
int wfm_writer_add_keyword(wfm_writer_state_t *w, const char *tag, char type,
                          const void *value, size_t count);

/**
 * @brief Flush, patch the BLUE data_size from the actual count (if seekable),
 *        write any attached extended-header keywords, and free the writer
 *        (does not close the FILE*).
 * @return 0 on success, non-zero on a write/seek error.
 */
int wfm_writer_close(wfm_writer_state_t *w);

/**
 * @brief Finalise and free — the object binding's fallible destructor.
 *
 * Identical to wfm_writer_close(); the object shape (gh-541) generates a
 * Python close() from this that raises when it returns non-zero, so the
 * finaliser's status reaches the caller and out of a `with` block. C callers
 * may use either name.
 *
 * @return 0 on success, non-zero on a write/seek error during finalisation.
 */
int wfm_writer_destroy(wfm_writer_state_t *state);

/* ── clip detection ───────────────────────────────────────────────────────
 * Full-scale is ±1.0 per axis; integer wire types saturate to it. The writer
 * always tracks the running peak |I|/|Q| (a fused max, free in the write loop),
 * so peak > 1.0 means an integer capture clipped — and the remedy is exactly
 * ceil(20*log10(peak)) dB of headroom. The per-component clipped *fraction* is
 * the one extra per-sample compare, so it is opt-in via
 * wfm_writer_track_clipping(); off, clip_fraction() returns 0. Float types
 * (cf32/cf64) never clip but still report a peak. Call after writing. */

/** Enable the per-component clip *counter* (off by default; peak is always on). */
void wfm_writer_track_clipping(wfm_writer_state_t *state, int on);

/* ── headroom ──────────────────────────────────────────────────────────────
 * A common output gain applied to every sample just before quantisation, so
 * peaks fit under full-scale. `--headroom H` (dB) backs the composite off to
 * −H dBFS: gain = 10^(−H/20). It is a single scale, so it does not change any
 * power ratio (SNR is invariant); it only moves the absolute level. Default
 * gain 1.0 (H = 0) is a bit-exact no-op (×1.0), so output stays byte-identical.
 * Floats scale too (they just never clip); peak/clip tracking sees the scaled
 * values. */

/** Set the output gain (linear; default 1.0). For headroom H dB pass 10^(−H/20). */
void wfm_writer_set_gain(wfm_writer_state_t *w, double gain);

/** Largest per-axis magnitude max(|I|,|Q|) written so far (pre-clip, full-scale
 *  1.0). > 1.0 ⇒ integer output clipped; peak_dBFS = 20*log10(peak). */
double wfm_writer_peak(const wfm_writer_state_t *w);

/** Fraction (0..1) of I/Q components that saturated (|v| > 1). Always 0 unless
 *  wfm_writer_track_clipping() was enabled. */
double wfm_writer_clip_fraction(const wfm_writer_state_t *w);

/**
 * @brief Open a capture for writing.
 *
 * Streams `complex64` blocks to disk in the chosen file type, wire sample type
 * and byte order, quantising to full scale (±1.0) for the integer types.
 * Finish with close(): a BLUE capture's `data_size` and extended header are
 * only written there, so a capture that is never closed is incomplete.
 *
 * @param path        where to write -- a `str` or any `os.PathLike` from
 *                    Python. For `file_type="sigmf"` this MUST end in
 *                    `.sigmf-data`: a SigMF capture is a
 *                    `<base>.sigmf-data` + `<base>.sigmf-meta` pair found by
 *                    name, and close() writes the sidecar beside it.
 * @param file_type   `"raw"` (headerless interleaved I/Q), `"csv"` (one
 *                    `I,Q` line per sample), `"blue"` (self-describing
 *                    X-Midas/REDHAWK type-1000) or `"sigmf"`. Only BLUE and
 *                    SigMF record `fs`/`fc`; raw and CSV have nowhere to put
 *                    them.
 * @param sample_type wire type: `"cf32"`, `"cf64"`, `"ci32"`, `"ci16"` or
 *                    `"ci8"`. The integer types quantise ±1.0 to full scale
 *                    and can clip -- see track_clipping()/peak_dbfs.
 * @param endian      `"le"` or `"be"`; ignored for CSV, which is text.
 * @param fs          sample rate (Hz). BLUE stores it as `xdelta = 1/fs`,
 *                    SigMF as `core:sample_rate`.
 * @param fc          centre frequency (Hz). BLUE records it as a `FREQ`
 *                    keyword, SigMF as `captures[0]["core:frequency"]`; raw
 *                    and CSV drop it. 0.0 writes nothing.
 * @param total       expected sample count, for the BLUE header; close()
 *                    patches the real count, so 0 is fine when unknown.
 * @param headroom    dB of output backoff (gain = 10^(-H/20)) applied before
 *                    quantisation. A single scale, so it does not change any
 *                    power ratio -- only the absolute level. 0 is a bit-exact
 *                    no-op.
 * @return a writer, or NULL if the path cannot be opened for writing (or is
 *         a SigMF path not ending in `.sigmf-data`).
 */
wfm_writer_state_t *wfm_writer_create(const char *path, int file_type, int sample_type, int endian, double fs, double fc, size_t total, double headroom);

/**
 * @brief Write a complete 512-byte BLUE/Platinum type-1000 Header Control Block.
 *
 * Used for the `blue` file type — both attached (the writer calls this with
 * `data_start = 512`, `detached = 0`, then streams the data after it) and
 * detached (the caller writes the data to a separate `.det` file and this HCB to
 * a `.hdr` file with `data_start = 0`, `detached = 1`). Every standard field is
 * written; the header byte order follows `endian`.
 *
 * @param fp            destination (binary).
 * @param sample_type   wire type (wavegen order) → BLUE format char C{B,I,L,F,D}.
 * @param endian        0 little (`EEEI`) / 1 big (`IEEE`).
 * @param fs            sample rate (Hz) → `xdelta = 1/fs`.
 * @param fc            centre frequency (Hz). Type 1000 has no HCB field for
 *                      it, so a non-zero value is written as an ASCII
 *                      `FREQ=<value>` pair in the HCB keyword area.
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

/* No wfm_writer_reset: the object declares `no_reset` (gh-542), so jm emits no
   reset() binding and no call site. A writer has nothing coherent to reset --
   the samples are on disk and the written count drives the BLUE data_size patch
   -- so the method is absent rather than a no-op or a raise. */
double wfm_writer_get_clip_fraction(const wfm_writer_state_t *state);
double wfm_writer_get_peak_dbfs(const wfm_writer_state_t *state);
bool wfm_writer_get_clipped(const wfm_writer_state_t *state);
int write_blue_header(const char *path, int sample_type, int endian, double fs, double fc, double data_start, size_t total, int detached);
#ifdef __cplusplus
}
#endif

#endif /* WFM_WRITER_H */
