/**
 * @file wfm_data.h
 * @brief A frame's data source: where a `data:LEN` payload's bits come from.
 *
 * A frame declares how many bits its payload carries, `data:LEN`, and a data
 * source supplies them, `LEN` at a time, one chunk per frame
 * (docs/design/payload-data-source.md). The source is one of:
 *
 * | built from                        | is a     | frames                    |
 * | --------------------------------- | -------- | ------------------------- |
 * | a literal Field (`0x…`, `0101`)   | finite   | `ceil(bits / LEN)`        |
 * | a generated Field with `LEN > 0`  | finite   | the same                  |
 * | `pn:0:REG[:SEED[:POLY]]`          | stream   | never ends, never pauses  |
 * | a regular file                    | finite   | `ceil(8 * bytes / LEN)`   |
 * | stdin, a pipe (any other fd)      | stream   | until the input ends      |
 *
 * `--data none` is not a source: it means no data at all (continuous DSSS
 * sends its code alone), so a face that reads `none` builds no source, and
 * @ref dp_wfm_data_create refuses the word as text outside the grammar.
 *
 * A file and a pipe carry **packed** octets, unpacked MSB first by
 * `dp_bytes_to_bin`. `LEN` need not be a multiple of 8: an octet can
 * straddle two frames, and the source keeps the leftover bits for the next.
 *
 * **Every refusal that can be decided before the first sample is decided at
 * create** (§4.4): a finite source whose length is not a multiple of `LEN`
 * needs a fill, a pipe needs one (its length is unknowable up front) unless
 * `LEN` is 1, and an empty finite source is refused. A one-bit frame has no
 * remainder to pad, and an idle frame with no fill is an error that ends
 * the run, so a one-bit pipe -- continuous DSSS reads one bit per data
 * symbol -- is complete without one, as long as nothing paces it. The last frame of a finite source
 * is padded with the fill, tiled from its first bit.
 *
 * **Each chunk has one of three outcomes** (§4.5): a frame of data, nothing
 * yet, or the end. *Nothing yet* happens only when a caller asks with a
 * timeout and a pipe has not delivered; the bits already read are kept for
 * the next frame, and it is the paced caller that decides to send an idle
 * frame instead, all fill (§4.1). An unpaced caller asks with no timeout and
 * waits, as `cat` does.
 *
 * A source read from an fd hashes every octet it reads with
 * `dp_hash64()`, so a record can identify the file without reading it
 * twice (§4.8).
 *
 * **A source resumes bit for bit when its bytes can be had again**
 * (@ref dp_wfm_data_get_state): a Field, `pn:0` and a regular file. A pipe
 * cannot -- the octets it has delivered are gone -- so its state is refused
 * at both ends, and @ref dp_wfm_data_state_refusal says why.
 */
#ifndef WFM_DATA_H
#define WFM_DATA_H

#include <stddef.h>
#include <stdint.h>

#include "doppler/dp_state.h"      /* DP_FOURCC, the state envelope */
#include "doppler/wfm/wfm_frame.h" /* wfm_seq_t */

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief What a request for the next frame's data produced. */
  typedef enum
  {
    WFM_DATA_FRAME   = 0, /**< a frame's data was written                  */
    WFM_DATA_NOT_YET = 1, /**< a pipe has not delivered; nothing written   */
    WFM_DATA_END     = 2, /**< the source has ended; nothing written       */
    WFM_DATA_ERROR   = 3, /**< a read failed (errno is set); nothing written */
    WFM_DATA_IDLE    = 4  /**< an idle frame, all fill, was written        */
  } wfm_data_status_t;

  /** @brief Whether frames are due at a time (`--realtime`) or on demand. */
  typedef enum
  {
    WFM_DATA_UNPACED = 0, /**< wait for the data, as `cat` does       */
    WFM_DATA_PACED   = 1  /**< never wait: nothing yet is an idle frame */
  } wfm_data_pacing_t;

  /** @brief Opaque; see @ref dp_wfm_data_create. */
  typedef struct wfm_data_src wfm_data_src_t;

  /** @brief What a source has done so far: the truth a record carries. */
  typedef struct
  {
    uint64_t frames;      /**< data frames written, the padded last included */
    uint64_t idle_frames; /**< idle frames written (@ref dp_wfm_data_idle)  */
    uint64_t pad_bits;    /**< fill bits in the padded last frame, else 0   */
    uint64_t bits;        /**< source bits consumed so far (no fill)        */
    uint64_t total_bits;  /**< a finite source's length; 0 for a stream     */
    uint64_t hash;        /**< `dp_hash64()` of the octets read (fd only)  */
    int      hashed;      /**< non-zero when @p hash is meaningful          */
    int      stream;      /**< non-zero for a stream (pipe, `pn:0`)         */
  } wfm_data_stats_t;

  /**
   * @brief Build a data source from a Field or from a path. NULL on refusal.
   *
   * Exactly one of @p data and @p path is given: `--data` and
   * `--data-from-file` are one exclusive pair (§4.9), and both is refused.
   *
   * @param data  a Field (`0x…`, `0101`, `pn:N:…`, or `pn:0:REG[:SEED[:POLY]]`
   *              for an endless seeded stream); NULL when @p path is given.
   *              `data:LEN` and `*REPS` on a stream are refused.
   * @param path  a file, or `-` for stdin; NULL when @p data is given.
   * @param len   bits per frame, `LEN` of the frame's `data:LEN`; > 0.
   * @param fill  a Field whose bits pad the last frame and fill an idle
   *              one, or NULL for none. `data:LEN` is refused as a fill.
   * @param why   optional; receives a STATIC sentence naming the cause of a
   *              refusal (a Field's is the parser's own), untouched on
   *              success. A remainder is stated in bits by the caller, from
   *              @ref dp_wfm_data_length_bits.
   * @return the source, or NULL: text outside the grammar, a file that
   *         cannot be opened, an empty finite source, a finite source that
   *         does not divide into `LEN`-bit frames with no fill, or a pipe
   *         with no fill and a `LEN` above 1.
   *
   * @code
   * const char     *why;
   * wfm_data_src_t *s = dp_wfm_data_create ("0xABCD", NULL, 8, NULL, &why);
   * uint8_t         b[8];
   * dp_wfm_data_next (s, 1, b, sizeof b, -1); // WFM_DATA_FRAME: 1010 1011
   * dp_wfm_data_next (s, 1, b, sizeof b, -1); // WFM_DATA_FRAME: 1100 1101
   * dp_wfm_data_next (s, 1, b, sizeof b, -1); // WFM_DATA_END
   * dp_wfm_data_destroy (s);
   * @endcode
   */
  wfm_data_src_t *dp_wfm_data_create (const char *data, const char *path,
                                      size_t len, const char *fill,
                                      const char **why);

  /**
   * @brief Build a data source over an open file descriptor. NULL on refusal.
   *
   * What @ref dp_wfm_data_create does for a path, over an fd the caller
   * already holds: a regular file is finite and its length comes from
   * `fstat`; anything else (a pipe, a terminal, a socket) is a stream and
   * needs @p fill. The source never closes @p fd.
   */
  wfm_data_src_t *dp_wfm_data_create_fd (int fd, size_t len,
                                         const char *fill, const char **why);

  /**
   * @brief A source's length in bits, known before it is built; 0 for a
   * stream.
   *
   * What a caller needs before the first sample: how many `LEN`-bit frames
   * a finite source makes, `ceil(bits / LEN)`, and -- when
   * @ref dp_wfm_data_create refuses a source that leaves a remainder with no
   * fill -- the numbers to state that remainder in. Exactly one of @p data
   * and @p path, as @ref dp_wfm_data_create takes them.
   *
   * @return the bits of a literal or generated Field, or of a regular file;
   *         0 for a stream (`pn:0:…`, `-`, a pipe), for text outside the
   *         grammar, and for a file that cannot be opened. A 0 does not say
   *         which, so call this after a successful @ref dp_wfm_data_create,
   *         where the source is known to exist and its stats say whether it
   *         is a stream.
   *
   * @code
   * const uint64_t n = dp_wfm_data_length_bits ("0xABC", NULL);   // 12
   * // in 8-bit frames: the last holds n % 8 = 4 and is 8 - 4 = 4 short
   * @endcode
   */
  uint64_t dp_wfm_data_length_bits (const char *data, const char *path);

  /**
   * @brief Build a data source from a source's own members. NULL on refusal.
   *
   * What @ref dp_wfm_data_create does from text, from the representation a
   * `wfm_source_t` carries: @p data is a `wfm_seq_t` -- a literal (a bit
   * array from Python, a parsed Field from the CLI or a scene) or a finite
   * generated sequence -- and @p fill likewise, rendered once through
   * `dp_wfm_seq_bits()`. Exactly one of @p data (with `len > 0`) and
   * @p path; the refusals are @ref dp_wfm_data_create's.
   *
   * @param data  the data sequence, or NULL / `len == 0` with @p path.
   * @param path  a file, or `-` for stdin; NULL with @p data.
   * @param len   bits per frame; > 0.
   * @param fill  the fill sequence, or NULL / `len == 0` for none.
   * @param why   optional; receives a STATIC reason for a refusal.
   *
   * @code
   * static const uint8_t bits[12] = { 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 0 };
   * const wfm_seq_t      data = { .kind = WFM_SEQ_LITERAL, .bits = bits,
   *                               .len = 12 };
   * const wfm_seq_t      fill = { .kind = WFM_SEQ_DOTTED, .len = 2 };
   * const char          *why;
   * wfm_data_src_t *s = dp_wfm_data_create_seq (&data, NULL, 8, &fill, &why);
   * if (!s)
   *   return 1;
   * wfm_data_stats_t st;
   * dp_wfm_data_stats (s, &st);
   * if (st.total_bits != 12) // two 8-bit frames, the second padded
   *   return 1;
   * dp_wfm_data_destroy (s);
   * @endcode
   */
  wfm_data_src_t *dp_wfm_data_create_seq (const wfm_seq_t *data,
                                          const char *path, size_t len,
                                          const wfm_seq_t *fill,
                                          const char **why);

  /** @brief Free a source; closes a file it opened itself. NULL is a no-op. */
  void dp_wfm_data_destroy (wfm_data_src_t *s);

  /**
   * @brief Write the next frame's data field: ONE chunk of `LEN` bits,
   * written @p reps times.
   *
   * A repeated data field (`data:LEN*REPS`) is one draw sent again, never
   * fresh draws (frame-description.md §F.1), so the source advances by
   * `LEN` bits per frame whatever @p reps is. The last chunk of a finite
   * source, or of a pipe that closes mid-chunk, is padded with the fill.
   *
   * @param s           the source.
   * @param reps        the field's repetitions; 0 means one.
   * @param out         receives `LEN * reps` bits, one per byte.
   * @param max_out     capacity of @p out in bits.
   * @param timeout_ms  how long to wait for a pipe: -1 waits for as long as
   *                    it takes (unpaced), 0 does not wait at all.
   * @return @ref WFM_DATA_FRAME, or @ref WFM_DATA_NOT_YET (nothing written;
   *         the bits already read are kept), @ref WFM_DATA_END (nothing
   *         written), or @ref WFM_DATA_ERROR (a read failed, or @p out is
   *         smaller than `LEN * reps`).
   */
  wfm_data_status_t dp_wfm_data_next (wfm_data_src_t *s, size_t reps,
                                      uint8_t *out, size_t max_out,
                                      int timeout_ms);

  /**
   * @brief Write an idle frame's data field: all fill, @p reps times.
   *
   * What a paced caller sends when @ref dp_wfm_data_next says
   * @ref WFM_DATA_NOT_YET, so the carrier and the frame timing never break.
   * It consumes no source bits (a partial chunk already read waits for the
   * next data frame) and is counted in `idle_frames`.
   *
   * @return @ref WFM_DATA_FRAME, or @ref WFM_DATA_ERROR when the source has
   *         no fill or @p out is too small.
   */
  wfm_data_status_t dp_wfm_data_idle (wfm_data_src_t *s, size_t reps,
                                      uint8_t *out, size_t max_out);

  /**
   * @brief The next frame's data field under a pacing: THE rule for idle
   * frames.
   *
   * §4.5 in one place: only a paced caller gets idle frames. Paced, the
   * source is asked without waiting, and *nothing yet* becomes an all-fill
   * idle frame (@ref dp_wfm_data_idle), so the carrier and the frame timing
   * never break. Unpaced, it waits for as long as the data takes and never
   * sends an idle frame. Every frame-pulling caller goes through this, so
   * no caller has to remember which timeout means which.
   *
   * @return @ref WFM_DATA_FRAME, @ref WFM_DATA_IDLE (paced only),
   *         @ref WFM_DATA_END or @ref WFM_DATA_ERROR.
   *
   * @code
   * const char     *why;
   * wfm_data_src_t *s = dp_wfm_data_create ("0xABCD", NULL, 16, "10", &why);
   * uint8_t         b[16];
   * // A Field never pauses, so this is a data frame: 1010 1011 1100 1101.
   * // A paced pipe with nothing yet would give WFM_DATA_IDLE, b all fill.
   * if (dp_wfm_data_frame (s, WFM_DATA_PACED, 1, b, sizeof b)
   *         != WFM_DATA_FRAME
   *     || b[0] != 1 || b[1] != 0)
   *   return 1;
   * if (dp_wfm_data_frame (s, WFM_DATA_PACED, 1, b, sizeof b) != WFM_DATA_END)
   *   return 1; // 16 bits in 16-bit frames: one frame, then the end
   * dp_wfm_data_destroy (s);
   * @endcode
   */
  wfm_data_status_t dp_wfm_data_frame (wfm_data_src_t *s,
                                       wfm_data_pacing_t pacing, size_t reps,
                                       uint8_t *out, size_t max_out);

  /** @brief What the source has done so far. */
  void dp_wfm_data_stats (const wfm_data_src_t *s, wfm_data_stats_t *out);

  /**
   * @brief A regular file's identity, as a record carries it: its length in
   * bits and the `dp_hash64()` of all its octets.
   *
   * What a source over the same file reports in its stats once it has read
   * to the end (`bits` and `hash`), computed here without building one. A
   * replay compares it with the record's and refuses a file that differs
   * (payload-data-source.md §4.8), before the first sample.
   *
   * @param path  a regular file; `-` and anything that is not a regular
   *              file (a pipe, a FIFO) have no identity to read up front.
   * @param bits  receives `8 * octets`; may be NULL.
   * @param h     receives the hash.
   * @return 0, or -1 when @p path cannot be opened, is not a regular file,
   *         or a read fails.
   *
   * @code
   * FILE *f = fopen ("ident.bin", "wb");
   * if (!f || fwrite ("foobar", 1, 6, f) != 6)
   *   return 1;
   * fclose (f);
   * uint64_t bits, h;
   * const int rc = dp_wfm_data_file_identity ("ident.bin", &bits, &h);
   * remove ("ident.bin");
   * if (rc != 0 || bits != 48 || h != UINT64_C (0x85944171f73967e8))
   *   return 1; // 6 octets, and FNV-1a 64 of "foobar"
   * if (dp_wfm_data_file_identity ("-", &bits, &h) != -1)
   *   return 1; // stdin has no identity to read up front
   * @endcode
   */
  int dp_wfm_data_file_identity (const char *path, uint64_t *bits,
                                 uint64_t *h);

/** @brief The data source's state blob type tag (dp_state.h). */
#define WFM_DATA_STATE_MAGIC DP_FOURCC ('W', 'F', 'D', 'S')
/** @brief The data source's state blob format version. */
#define WFM_DATA_STATE_VERSION 1u

  /**
   * @brief Why a source's state cannot be serialized, or NULL when it can.
   *
   * A pipe (stdin, a FIFO, a socket) is the one source that cannot resume:
   * the octets it has delivered are gone, so a blob could only restart it
   * from wherever the pipe is now -- different data under the same frame
   * count. It is refused at BOTH ends: @ref dp_wfm_data_state_bytes is 0,
   * so a checkpoint fails when it is taken rather than on the pod that
   * relies on it, and @ref dp_wfm_data_set_state is `DP_ERR_INVALID`.
   *
   * @return NULL, or a STATIC sentence naming the cause.
   */
  const char *dp_wfm_data_state_refusal (const wfm_data_src_t *s);

  /**
   * @brief Bytes in the source's state blob; 0 when it refuses
   * (@ref dp_wfm_data_state_refusal).
   *
   * The blob carries only what RUNS -- the counts, the end latch, and the
   * position: a Field's cursor, `pn:0`'s register (a nested `dp_pn` blob),
   * or a file's residue and running hash. The bits, the fill and `LEN` are
   * config, rebuilt by the create that built the receiving source.
   */
  size_t dp_wfm_data_state_bytes (const wfm_data_src_t *s);

  /**
   * @brief Serialize the source into @p blob, of
   * @ref dp_wfm_data_state_bytes bytes. A no-op when it refuses.
   */
  void dp_wfm_data_get_state (const wfm_data_src_t *s, void *blob);

  /**
   * @brief Restore a source built with the same config from @p blob.
   *
   * The receiving source must be the same kind over the same length in
   * the same `LEN`. A file is re-read from its start up to the blob's
   * octet offset and the `dp_hash64` of that prefix must equal the blob's:
   * a file that changed under the checkpoint is refused, never resumed
   * into different data. Nothing changes on a refusal.
   *
   * @return DP_OK, or DP_ERR_INVALID: a bad envelope, another kind, size or
   *         `LEN`, a pipe, a position past the end, or a prefix whose hash
   *         differs.
   *
   * @code
   * const char     *why;
   * wfm_data_src_t *a = dp_wfm_data_create ("0xABCD", NULL, 8, NULL, &why);
   * wfm_data_src_t *b = dp_wfm_data_create ("0xABCD", NULL, 8, NULL, &why);
   * uint8_t         x[8], y[8], blob[256];
   * if (dp_wfm_data_state_bytes (a) > sizeof blob)
   *   return 1;
   * dp_wfm_data_next (a, 1, x, sizeof x, -1); // 1010 1011
   * dp_wfm_data_get_state (a, blob);
   * if (dp_wfm_data_set_state (b, blob) != DP_OK)
   *   return 1;
   * dp_wfm_data_next (a, 1, x, sizeof x, -1); // 1100 1101
   * dp_wfm_data_next (b, 1, y, sizeof y, -1); // the same frame
   * if (memcmp (x, y, sizeof x) != 0)
   *   return 1;
   * dp_wfm_data_destroy (a);
   * dp_wfm_data_destroy (b);
   * @endcode
   */
  int dp_wfm_data_set_state (wfm_data_src_t *s, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* WFM_DATA_H */
