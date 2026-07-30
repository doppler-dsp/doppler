/**
 * @file wfm_reader_core.h
 * @brief Input file types for generated IQ — the dual of wfm_writer.
 *
 * Reads back what wfm_writer wrote: raw interleaved I/Q, CSV, BLUE type-1000
 * (attached or detached, `format` mode `S` or `C`), and SigMF. A BLUE file in
 * any other mode is rejected at open — see ::wfm_mode_t.
 *
 * The file type is **auto-detected from the file's CONTENT**, not its name: the
 * BLUE magic at byte 0, a first line that parses as `I,Q` for CSV, a
 * `.sigmf-meta` sidecar alongside. The extension only breaks a tie the content
 * cannot (a `.det` payload, which is headerless by construction). So a CSV
 * called `capture.dat` reads as CSV, and a BLUE file called `capture.csv` reads
 * as BLUE — misnaming a capture costs nothing.
 *
 * Self-describing file types (BLUE, SigMF) recover the sample type, byte order,
 * sample rate and centre frequency from their metadata. Headerless file types
 * (raw, CSV) take the sample type / byte order as hints, and there is no way to
 * check a hint against the file — see ::wfm_reader_get_trailing_bytes for the
 * one tell that is available.
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
 * while ((n = wfm_reader_read(r, 4096, buf, 4096)) > 0)   // (state, count, out)
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
   *  misinterpreted as interleaved I/Q. Non-BLUE file types are complex. */
  typedef enum
  {
    WFM_MODE_COMPLEX = 0, /**< 'C' — interleaved I/Q, two components. */
    WFM_MODE_SCALAR = 1   /**< 'S' — real, one component (Q read as 0). */
  } wfm_mode_t;

  /** Where a capture's centre frequency came from.
   *
   *  BLUE type-1000 has **no HCB field for centre frequency** — the adjunct
   *  carries `xstart`/`xdelta`/`xunits`, which describe the abscissa (time),
   *  not the RF the capture was taken at. So an RF capture conveys it as a
   *  keyword, and which tag it uses is X-Midas convention rather than anything
   *  BLUE 1.1 mandates: 3.1.2.6.4.4 defines `FREQ`, but only as a type-6000
   *  *column* name, under a heading stating those names "are not keyword
   *  names". `FREQ` in the HCB keyword area is nonetheless what real captures
   *  carry, so it is what this reader looks for first.
   *
   *  Reporting the tag matters because 0.0 is a legitimate answer: a genuine
   *  baseband capture and a capture whose frequency this library failed to
   *  find are otherwise indistinguishable. ::WFM_FC_NONE says "not found",
   *  and only then is `fc == 0.0` a guess rather than a reading. */
  typedef enum
  {
    WFM_FC_NONE = 0,      /**< no source found; `fc` is 0.0 by default. */
    WFM_FC_FREQ,          /**< `FREQ` keyword — the usual X-Midas tag. */
    WFM_FC_RF_FREQ,       /**< `RF_FREQ` keyword. */
    WFM_FC_CENTER_FREQ,   /**< `CENTER_FREQ` keyword. */
    WFM_FC_F_C,           /**< `F_C` keyword. */
    WFM_FC_SIGMF          /**< SigMF `captures[0]["core:frequency"]`. */
  } wfm_fc_source_t;

  /** Resolved metadata for an open capture. Fields the file type does not
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
    int    fc_source;   /**< wfm_fc_source_t: where `fc` was read from. */
    size_t trailing_bytes; /**< payload bytes past the last whole sample. */
  } wfm_reader_info_t;

  /**
   * @brief Open a capture, auto-detecting its file type from its content.
   *
   * Detection order, first match wins: the BLUE magic at byte 0; a first
   * line that scans as `I,Q`; otherwise headerless raw. Two suffixes are
   * decided by name instead, because neither has content that identifies
   * it — `.det`, a detached payload described by its header sibling, and
   * `.sigmf-data`, half of a pair whose other half carries the datatype.
   *
   * Nothing is refused for looking unfamiliar: an unrecognised file opens as
   * raw at the caller's @p sample_type, because a truncated or partial
   * recording is a real thing and a reader that rejects it is useless. What
   * you get instead of a refusal is ::wfm_reader_get_trailing_bytes.
   *
   * @param path           file to read -- a `str` or any `os.PathLike` from
   *                       Python. For a DETACHED BLUE capture this is
   *                       normally the HEADER file -- `<base>.tmp` or
   *                       `<base>.prm` per BLUE 3.1.1.4 (this library's own
   *                       writer emits `<base>.hdr`) -- whose HCB `detached`
   *                       field points at the collocated `<base>.det`
   *                       payload; the extension does not decide, `detached`
   *                       does. Passing the `<base>.det` directly also works
   *                       (its header sibling is resolved). A SigMF
   *                       `.sigmf-data` file resolves its `.sigmf-meta`
   *                       sidecar the same way.
   * @param sample_type    the wire sample type, used only as a HINT for the
   *                       headerless file types (raw, CSV) -- BLUE and SigMF
   *                       carry their own and ignore it. `"cf32"`, `"cf64"`,
   *                       `"ci32"`, `"ci16"` or `"ci8"` from Python; the
   *                       matching 0..4 from C. A wrong hint does not fail;
   *                       see ::wfm_reader_get_trailing_bytes.
   * @param endian         byte order, likewise a hint that only headerless
   *                       raw uses; `"le"` or `"be"` from Python, 0 or 1
   *                       from C.
   * @return a reader, or NULL on open/parse failure.
   */
wfm_reader_state_t *wfm_reader_create(const char *path, int sample_type, int endian);

  /** @brief Copy the resolved capture metadata into @p info. */
  void wfm_reader_info (const wfm_reader_state_t *r, wfm_reader_info_t *info);

  /**
   * @brief Read up to @p count samples, returning them as `complex64`.
   *
   * Samples come out at unit scale whatever the wire type was: a float type
   * is reinterpreted, an integer type is divided by its full scale. Returns
   * fewer than asked at the end of the capture, and 0 once it is exhausted,
   * so a `while` over the result terminates. Never returns more than the
   * file's declared payload — trailing bytes past `data_size` (an extended
   * header, X-Midas slack) are not samples.
   *
   * @param state   the reader.
   * @param n       how many samples to read (`count` in the Python binding,
   *                which also accepts an optional pre-allocated `out=` array
   *                to avoid an allocation per block in a streaming loop).
   * @param out     destination, at least @p max_out samples.
   * @param max_out capacity of @p out; emission stops there.
   *
   * @code
   * >>> import pathlib, tempfile
   * >>> from doppler.wfm import Composer, Reader, Segment, Writer
   * >>> tmp = tempfile.TemporaryDirectory()
   * >>> p = pathlib.Path(tmp.name) / "capture.blue"
   * >>> x = Composer([Segment("qpsk", sps=8, num_samples=1024)]).compose()
   * >>> with Writer(p, file_type="blue", sample_type="ci16",
   * ...             fs=2.4e6, fc=1.2e9) as w:
   * ...     _ = w.write(x)
   * >>> r = Reader(p)
   * >>> r.file_type, r.sample_type, r.endian
   * ('blue', 'ci16', 'le')
   * >>> r.fs, r.fc, r.fc_source
   * (2400000.0, 1200000000.0, 'FREQ')
   * >>> total = 0
   * >>> while len(block := r.read(256)):
   * ...     total += len(block)
   * >>> total
   * 1024
   * >>> r.close()
   * >>> tmp.cleanup()   # directory and contents removed
   * @endcode
   */
size_t wfm_reader_read(wfm_reader_state_t *state, size_t n,
                       float complex *out, size_t max_out);

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
size_t wfm_reader_num_keywords(const wfm_reader_state_t *state);

  /**
   * @brief The @p i'th keyword in file order, or NULL if @p i is out of range.
   *
   * The returned pointer (and its `value` buffer) is owned by the reader and
   * is freed by wfm_reader_destroy().
   */
  const wfm_keyword_t *wfm_reader_keyword (const wfm_reader_state_t *r, size_t i);

  /**
   * @brief The tag of the @p i'th keyword (key_fn for the `.keywords` dict).
   *
   * jm's generated dict loop (gh-543) calls this for every index in
   * [0, wfm_reader_num_keywords()), so @p i is always in range. The returned
   * pointer is owned by the reader.
   */
const char *wfm_reader_keyword_tag(const wfm_reader_state_t *state, size_t i);

  /**
   * @brief The first keyword whose tag equals @p tag, or NULL if absent.
   *
   * Tags are not required to be unique; this returns the earliest match.
   */
  /**
   * @brief Number of decoded HCB fields (0 for a non-BLUE file type).
   */
  size_t wfm_reader_num_header_fields(const wfm_reader_state_t *state);

  /**
   * @brief The i-th decoded HCB field, or NULL if @p i is out of range.
   *
   * Every field of the 512-byte header control block is carried as a
   * `wfm_keyword_t`, under the name the format itself uses -- `data_start`,
   * `ext_size`, `xdelta` and so on (Midas BLUE 1.1 3.1.1). Reusing the
   * keyword struct means the header and the keywords share one tag/value
   * codec, so a double or an ASCII field can never be turned into a Python
   * object two different ways.
   */
  const wfm_keyword_t *wfm_reader_header_field(const wfm_reader_state_t *state,
                                               size_t i);

  /**
   * @brief The i-th HCB field's name, for the `.header` dict binding.
   */
  const char *wfm_reader_header_tag(const wfm_reader_state_t *state, size_t i);

  /**
   * @brief Look up one HCB field by name, or NULL if absent.
   */
  const wfm_keyword_t *
  wfm_reader_find_header_field(const wfm_reader_state_t *state,
                               const char *name);

  const wfm_keyword_t *wfm_reader_find_keyword (const wfm_reader_state_t *r,
                                                const char        *tag);

  /**
   * @brief Rewind to the first sample of the capture.
   *
   * Seeks back to where the payload starts — 512 bytes into an attached BLUE
   * file, byte 0 of a `.det` or a raw/SigMF payload — and restores the
   * remaining-sample count, so the capture reads again from the top. The
   * file's metadata and decoded keywords are unaffected: they came from the
   * header and do not change.
   */
void wfm_reader_reset(wfm_reader_state_t *state);

  /** @brief Close the file, free the reader and its decoded keywords. */
void wfm_reader_destroy(wfm_reader_state_t *state);

  /**
   * @brief Which keyword ::wfm_reader_get_fc read the centre frequency from.
   *
   * A ::wfm_fc_source_t. ::WFM_FC_NONE means nothing was found, which is the
   * only way to tell a baseband capture (`fc` genuinely 0 Hz) from one whose
   * frequency this library could not locate — both report `fc == 0.0`.
   */
int wfm_reader_get_fc_source(const wfm_reader_state_t *state);

  /**
   * @brief Payload bytes left over after the last whole sample.
   *
   * A capture is a whole number of samples, so this is 0 for every file whose
   * declared sample type and mode match its content. Non-zero means one of
   * two things, and the reader cannot tell them apart:
   *
   * - the `sample_type`/`endian` hint is wrong for a headerless file type
   *   (reading a `ci16` file as `cf32` leaves a remainder unless the length
   *   happens to divide), or
   * - the capture is truncated — a recording that was cut mid-sample.
   *
   * Either way the leftover bytes are dropped: ::wfm_reader_read stops at the
   * last complete sample. This exists because there is otherwise no signal at
   * all. A wrong hint on a headerless file does not fail, it returns
   * plausible garbage at the wrong stride, and nothing in the samples
   * themselves says so.
   *
   * Always 0 for CSV, which is delimited rather than strided.
   */
size_t wfm_reader_get_trailing_bytes(const wfm_reader_state_t *state);

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
