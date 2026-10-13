/**
 * @file spectrogram_core.h
 * @brief Streaming spectrogram: a stream of any-size chunks in, rows of
 *        nfft-bin spectra out, one row every hop samples.
 *
 * A row is the PSD of one frame, and nothing else: row k is PSD's per-frame
 * kernel applied to stream samples [k*hop, k*hop + nfft) --
 * dp_psd_frame_linear() for power rows, the default, or dp_psd_frame_db()
 * for dB rows, which a caller asks for by name. So the window, the FFT and
 * the full-scale reference are PSD's own, and a full-scale tone on a bin
 * reads 1.0 in power, 0 dB in dB. The rows are a function of the INPUT
 * STREAM, not of how it was split into calls: pushing it in one call, a
 * sample at a time, or in any other partition gives the same rows, bit for
 * bit.
 *
 * The object composes and re-implements none of its parts. The carry between
 * calls is the ring's framed face (DECLARE_DP_BUFFER_FRAMES): fewer than nfft
 * samples are held once a push returns, so the state blob has a size that
 * depends on nfft alone. The spectrum is PSD's per-frame kernel, held by
 * value (dp_psd_kernel_t): the window, the transform and the full-scale
 * reference, and none of PSD's averaging. It does not average rows (fold
 * them with AccTrace), detect, display or decimate.
 *
 * Lifecycle: dp_spectrogram_create(), then any number of
 * dp_spectrogram_push() calls, then dp_spectrogram_flush() once to end the
 * stream, then dp_spectrogram_destroy(). A short output buffer never loses
 * input: push stops at a whole row and dp_spectrogram_consumed() says where
 * to resume.
 *
 * Not thread-safe on one object (the kernel uses the object's scratch).
 * Complex float32 input only. The design, its goals and what it costs are
 * docs/design/spectrogram.md. The C examples are native/examples/
 * spectrogram_demo.c, which the example gate runs; they are not kept in this
 * header, because the Python docstrings are generated from it.
 */
#ifndef DP_SPECTROGRAM_CORE_H
#define DP_SPECTROGRAM_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/f32_buffer/f32_buffer_core.h"
#include "doppler/psd/psd_core.h"
#include <stddef.h>
#include <stdint.h>
#include "doppler/dp_interrupt_guard/dp_interrupt_guard_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief State-blob magic ('SPGM') and layout version. */
#define SPECTROGRAM_STATE_MAGIC DP_FOURCC ('S', 'P', 'G', 'M')
/* v2: the blob carries the configuration (mode, nfft, hop, window, beta) so a
 * restore refuses a blob whose rows would mean something else (#2102). */
#define SPECTROGRAM_STATE_VERSION 2u

/* The row units. The Python face binds "power" and "db" to these constants
 * by name (just-makeit.toml, `[[enum]] spectrogram_mode`), so their order is
 * this header's alone. */
/* SSOT: enum=spectrogram_mode */
enum
{
  /**
   * @brief Row units: linear power against the PSD's full-scale reference.
   *        The DEFAULT: what every example, guide and binding uses unless
   *        it asks for dB by name.
   *
   * Row k is dp_psd_frame_linear() of its frame, so a full-scale tone on a
   * bin reads 1.0 whatever the window, and an all-zero frame reads 0: the
   * only floor is float32's. A power row is what an averaging consumer
   * folds with AccTrace (the mean of dB rows is not the dB of the mean), and
   * what a display converts to dB for only the bins it draws. It is the
   * default because that conversion is most of a dB row's cost: 70-83% of
   * it at nfft 256 to 65536 (docs/design/spectrogram-measurements.md
   * section 5.8).
   *
   * It is 0, so a zeroed or calloc'd configuration selects power, never dB:
   * dB is only ever asked for.
   */
  DP_SPECTROGRAM_POWER = 0,
  /**
   * @brief Row units: dBFS, against the same reference. Asked for by name.
   *
   * Row k is dp_psd_frame_db() of its frame: 10*log10 of the quotient a
   * power row holds, so a full-scale tone on a bin reads 0 dB. A bin reads
   * no lower than -200 dB: PSD clamps power at 1e-20 before the log, so an
   * all-zero frame and a frame below the floor write the same row.
   */
  DP_SPECTROGRAM_DB = 1
};

/**
 * @brief Spectrogram state. Allocate with dp_spectrogram_create().
 *
 * Every field is configuration (or @c k's scratch) except @c fr's stream
 * position and @c consumed. The state blob carries the configuration a row depends on
 * (mode, nfft, hop, window, beta) and the stream position, not @c consumed.
 */
typedef struct
{
  dp_psd_kernel_t k;   /**< PSD's per-frame kernel: n = nfft, no pad.  */
  dp_f32_t *ring;      /**< The carry's storage, owned by @c fr alone.  */
  dp_f32_framer_t fr;  /**< Any chunk in, nfft-sample frames out.       */
  float _Complex *last; /**< flush()'s zero-padded frame, nfft samples. */
  size_t nfft;          /**< Samples per frame, and bins per row.       */
  size_t hop;           /**< Samples between row starts.                */
  int window;           /**< A DP_PSD_WINDOW_* (the kernel's window).  */
  float beta;           /**< Kaiser beta (the Kaiser window only).      */
  int mode;             /**< DP_SPECTROGRAM_POWER or DP_SPECTROGRAM_DB. */
  size_t consumed;      /**< Input samples the last push took.          */
} dp_spectrogram_state_t;

/**
 * @brief Create a streaming spectrogram.
 *
 * @param nfft    Samples per frame and bins per row, which must be the
 *                same number: an nfft that would be zero-padded to a longer
 *                transform (anything but a power of two >= 2) is refused
 *                rather than given rows wider than its frames.
 * @param hop     Samples between row starts, 1 to nfft. 0 means nfft, which
 *                tiles the stream; the default. Less than nfft overlaps the
 *                frames. A hop above nfft would leave samples no row covers,
 *                so it is refused.
 * @param window  The window: "hann", "kaiser", "blackman-harris" or "rect",
 *                as PSD takes them. It also refuses a window that sums to
 *                zero at this nfft (the symmetric Hann at nfft = 2).
 * @param beta    Kaiser beta (ignored for the other windows).
 * @param mode    Row units: "power" (linear rows, the default) or "db" (dBFS
 *                rows, asked for by name). Any other value is refused.
 * @return Heap-allocated state, or NULL on an invalid argument.
 *
 * Every row is DC-centred exactly as PSD's kernel emits it: bin k at index
 * nfft/2 + k, negative frequencies first.
 * @note Call dp_spectrogram_destroy() when done.
 */
dp_spectrogram_state_t *dp_spectrogram_create (size_t nfft, size_t hop,
                                               int window, float beta,
                                               int mode);

/**
 * @brief Release a spectrogram and everything it owns.
 * @param s  May be NULL (no-op).
 */
void dp_spectrogram_destroy (dp_spectrogram_state_t *s);

/**
 * @brief Forget the stream: drop the carry and restart at sample 0.
 *
 * The next row covers samples [0, nfft) of whatever is pushed next.
 * The consumed count and the pending count read 0.
 * Configuration is kept.
 *
 * @param s  Must be non-NULL.
 */
void dp_spectrogram_reset (dp_spectrogram_state_t *s);

/**
 * @brief Push input, write every whole row it completes that fits.
 *
 * Takes the input in order and produces each row as soon as a frame of nfft
 * samples is complete, at every hop. A partial frame is carried to the next
 * call, so the rows do not depend on how the input was split into calls. Input
 * that completes no row is always taken whole.
 *
 * Only whole rows are written. A C caller bounds the write with @p max_out and
 * takes the rows that fit; the consumed count reports the input those rows
 * used, and the caller offers the rest again. The Python face has no short
 * buffer: its binding sizes the output for the whole chunk (see the Python
 * method doc).
 *
 * @param s        Must be non-NULL.
 * @param in       Complex baseband samples (cf32).
 * @param n_in     Samples in @p in.
 * @param out      Rows, row-major, nfft floats each.
 * @param max_out  Floats @p out has room for.
 * @return Floats written, a multiple of nfft. From Python, the completed
 *         rows as a (rows, nfft) matrix, or None when none is completed.
 */
size_t dp_spectrogram_push (dp_spectrogram_state_t *s,
                            const float _Complex *in, size_t n_in, float *out,
                            size_t max_out);

/**
 * @brief Floats one push of @p n_in samples writes when @p out has room:
 *        the rows_for(n_in) count times nfft.
 *
 * The capacity that makes a push take ALL of its input. Saturates at
 * SIZE_MAX rather than wrapping.
 *
 * @param s     Must be non-NULL.
 * @param n_in  Samples about to be pushed.
 */
size_t dp_spectrogram_push_max_out (const dp_spectrogram_state_t *s,
                                    size_t n_in);

/**
 * @brief Input samples the last dp_spectrogram_push() took.
 *
 * Equal to its @p n_in unless @p out ran out of room; then the caller
 * resumes at in + consumed. 0 after create, reset, flush and set_state.
 *
 * @param s  Must be non-NULL.
 */
size_t dp_spectrogram_consumed (const dp_spectrogram_state_t *s);

/**
 * @brief Rows a push of @p n_in more samples completes, given the carry:
 *        exact, not an estimate.
 *
 * The carry plus @p n_in samples, cut into frames of nfft at the hop. A push
 * with room for this many rows writes exactly this many and takes all of
 * @p n_in.
 *
 * @param s     Must be non-NULL.
 * @param n_in  Samples about to be pushed.
 */
size_t dp_spectrogram_rows_for (const dp_spectrogram_state_t *s,
                                size_t n_in);

/**
 * @brief End the stream: write the one zero-padded row it still owes, if any.
 *
 * The row sits on the hop grid: it starts at the next row start k*hop, never
 * at the first sample no row covered, so it is the row a one-shot push of
 * the input zero-padded to that row's end would have written. It is written
 * if and only if the stream holds a sample no earlier row covered. Either
 * way the spectrogram then restarts at sample 0, as after reset, so a second
 * flush writes nothing.
 *
 * @param s        Must be non-NULL.
 * @param row      Room for nfft floats.
 * @param row_len  Floats of room in @p row. Must be exactly nfft: any other
 *                 length is refused before anything moves, the owed frame
 *                 stays owed, and -1 is returned (never a count).
 * @return Floats written: nfft, or 0 if no row was owed; -1 if the row is not
 *         exactly nfft floats long. From Python, the owed row as a (1, nfft)
 *         matrix, or None when none was owed.
 */
int64_t dp_spectrogram_flush (dp_spectrogram_state_t *s, float *row,
                              size_t row_len);

/**
 * @brief Floats dp_spectrogram_flush() writes: nfft, the one row it can owe.
 *
 * Sized by the object alone, since a flush owes at most one row. It is also
 * the only row length dp_spectrogram_flush() accepts.
 *
 * @param s  Must be non-NULL.
 */
size_t dp_spectrogram_flush_max_out (const dp_spectrogram_state_t *s);

/**
 * @brief Samples pushed that no written row has covered yet.
 *
 * What dp_spectrogram_flush() would turn into a row: 0 means the stream is
 * complete as it stands.
 *
 * @param s  Must be non-NULL.
 */
size_t dp_spectrogram_pending (const dp_spectrogram_state_t *s);

/**
 * @brief Bytes of the state blob, a function of nfft alone.
 *
 * The blob is the configuration a row depends on -- mode, nfft, hop, window
 * and beta -- then the stream position: the framer's snapshot, which is the
 * carry (fewer than nfft samples, padded to nfft - 1), its element size, the
 * stream counts and the hop that frames them. Both sit inside the
 * spectrogram's own envelope, version 2. Every field has a fixed size, so the
 * length depends on nfft alone. It does not carry consumed(), which reads 0
 * after a restore. What a restore refuses is dp_spectrogram_set_state()'s.
 *
 * @param s  Must be non-NULL.
 */
size_t dp_spectrogram_state_bytes (const dp_spectrogram_state_t *s);

/**
 * @brief Serialize the configuration a row depends on, then the stream
 *        position, into @p blob.
 * @param s     Must be non-NULL.
 * @param blob  dp_spectrogram_state_bytes(s) bytes, every one written.
 */
void dp_spectrogram_get_state (const dp_spectrogram_state_t *s, void *blob);

/**
 * @brief Restore the stream position from a blob of the same configuration,
 *        so the next push continues it bit for bit.
 *
 * The blob is refused, with @p s unchanged, when its magic, version or size
 * is wrong; when the configuration it carries differs from @p s's -- another
 * mode, nfft, hop or window, or under the Kaiser window another beta, since a
 * checkpoint from another configuration would mean something else (#2102);
 * or when its carry is corrupt. A beta under any other window changes no row,
 * so it is not compared. A version-1 blob, which carried no configuration, is
 * refused by its version.
 *
 * @param s     A spectrogram.
 * @param blob  From dp_spectrogram_get_state().
 * @return DP_OK, or DP_ERR_INVALID.
 */
int dp_spectrogram_set_state (dp_spectrogram_state_t *s, const void *blob);
#ifdef __cplusplus
}
#endif

#endif /* DP_SPECTROGRAM_CORE_H */
