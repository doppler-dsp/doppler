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
 * A file and a pipe carry **packed** octets, unpacked MSB first by
 * `dp_bytes_to_bin`. `LEN` need not be a multiple of 8: an octet can
 * straddle two frames, and the source keeps the leftover bits for the next.
 *
 * **Every refusal that can be decided before the first sample is decided at
 * create** (§4.4): a finite source whose length is not a multiple of `LEN`
 * needs a fill, a pipe always needs one (its length is unknowable up front),
 * and an empty finite source is refused. The last frame of a finite source
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
 * @ref dp_hash64, so a record can identify the file without reading it
 * twice (§4.8).
 *
 * Not in this object yet: the state triplet (a stream's resume position),
 * which lands with the faces that serialize it.
 */
#ifndef WFM_DATA_H
#define WFM_DATA_H

#include <stddef.h>
#include <stdint.h>

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
    WFM_DATA_ERROR   = 3  /**< a read failed (errno is set); nothing written */
  } wfm_data_status_t;

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
    uint64_t hash;        /**< @ref dp_hash64 of the octets read (fd only)  */
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
   * @param why     optional; receives a sentence naming the cause of a
   *                refusal, with its numbers (a remainder is stated in
   *                bits). Untouched on success.
   * @param why_cap capacity of @p why in bytes, NUL included.
   * @return the source, or NULL: text outside the grammar, a file that
   *         cannot be opened, an empty finite source, a finite source that
   *         does not divide into `LEN`-bit frames with no fill, or a pipe
   *         with no fill.
   *
   * @code
   * char            why[160];
   * wfm_data_src_t *s
   *     = dp_wfm_data_create ("0xABCD", NULL, 8, NULL, why, sizeof why);
   * uint8_t         b[8];
   * dp_wfm_data_next (s, 1, b, sizeof b, -1); // WFM_DATA_FRAME: 1010 1011
   * dp_wfm_data_next (s, 1, b, sizeof b, -1); // WFM_DATA_FRAME: 1100 1101
   * dp_wfm_data_next (s, 1, b, sizeof b, -1); // WFM_DATA_END
   * dp_wfm_data_destroy (s);
   * @endcode
   */
  wfm_data_src_t *dp_wfm_data_create (const char *data, const char *path,
                                      size_t len, const char *fill,
                                      char *why, size_t why_cap);

  /**
   * @brief Build a data source over an open file descriptor. NULL on refusal.
   *
   * What @ref dp_wfm_data_create does for a path, over an fd the caller
   * already holds: a regular file is finite and its length comes from
   * `fstat`; anything else (a pipe, a terminal, a socket) is a stream and
   * needs @p fill. The source never closes @p fd.
   */
  wfm_data_src_t *dp_wfm_data_create_fd (int fd, size_t len,
                                         const char *fill, char *why,
                                         size_t why_cap);

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

  /** @brief What the source has done so far. */
  void dp_wfm_data_stats (const wfm_data_src_t *s, wfm_data_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif /* WFM_DATA_H */
