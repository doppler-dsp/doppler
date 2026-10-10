/**
 * @file spectrogram_core.h
 * @brief Streaming spectrogram: a stream of any-size chunks in, rows of
 *        nfft-bin spectra out, one row every hop samples.
 *
 * A row is the PSD of one frame, and nothing else: row k is
 * dp_psd_frame_db() of stream samples [k*hop, k*hop + nfft), so the window,
 * the FFT and the dBFS reference are PSD's own and a full-scale tone on a bin
 * reads 0 dB. The rows are a function of the INPUT STREAM, not of how it was
 * split into calls: pushing it in one call, a sample at a time, or in any
 * other partition gives the same rows, bit for bit.
 *
 * The object composes and re-implements none of its parts. The carry between
 * calls is the ring's framed face (DECLARE_DP_BUFFER_FRAMES): fewer than nfft
 * samples are held once a push returns, so the state blob has a size that
 * depends on nfft alone. The spectrum is PSD's per-frame kernel. It does not
 * average rows (fold them with AccTrace), detect, display or decimate.
 *
 * Lifecycle: dp_spectrogram_create(), then any number of
 * dp_spectrogram_push() calls, then dp_spectrogram_flush() once to end the
 * stream, then dp_spectrogram_destroy(). A short output buffer never loses
 * input: push stops at a whole row and dp_spectrogram_consumed() says where
 * to resume.
 *
 * Not thread-safe on one object (the kernel uses the object's scratch).
 * Complex float32 input only. The design, its goals and what is still
 * unmeasured are docs/design/spectrogram.md.
 */
#ifndef DP_SPECTROGRAM_CORE_H
#define DP_SPECTROGRAM_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/f32_buffer/f32_buffer_core.h"
#include "doppler/psd/psd_core.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief State-blob magic ('SPGM') and layout version. */
#define SPECTROGRAM_STATE_MAGIC DP_FOURCC ('S', 'P', 'G', 'M')
#define SPECTROGRAM_STATE_VERSION 1u

/** @brief Row units: dBFS, against the PSD's full-scale reference. */
#define DP_SPECTROGRAM_DB 0
/**
 * @brief Row units: linear power. RESERVED: dp_spectrogram_create() refuses
 *        it until PSD's normalised per-frame power is on main, so that a
 *        power row and a dB row share one reference.
 */
#define DP_SPECTROGRAM_POWER 1

/**
 * @brief Spectrogram state. Allocate with dp_spectrogram_create().
 *
 * Every field is configuration except @c fr's stream position and
 * @c consumed; the state blob carries only the former.
 */
typedef struct
{
  dp_psd_state_t *psd; /**< The per-frame kernel: n = nfft, no zero-pad. */
  dp_f32_t *ring;      /**< The carry's storage, owned by @c fr alone.  */
  dp_f32_framer_t fr;  /**< Any chunk in, nfft-sample frames out.       */
  float _Complex *last; /**< flush()'s zero-padded frame, nfft samples. */
  size_t nfft;          /**< Samples per frame, and bins per row.       */
  size_t hop;           /**< Samples between row starts.                */
  int window;           /**< PSD window index (see dp_psd_create()).    */
  float beta;           /**< Kaiser beta (window 1 only).               */
  int mode;             /**< DP_SPECTROGRAM_DB.                         */
  size_t consumed;      /**< Input samples the last push took.          */
} dp_spectrogram_state_t;

/**
 * @brief Create a streaming spectrogram.
 *
 * @param nfft    Samples per frame and bins per row, which must be the
 *                same number: an nfft the PSD would zero-pad to a longer
 *                transform (anything but a power of two >= 2) is refused
 *                rather than given rows wider than its frames.
 * @param hop     Samples between row starts, 1 <= hop <= nfft. hop == nfft
 *                tiles the stream; hop < nfft overlaps the frames.
 * @param window  0 = Hann, 1 = Kaiser, 2 = Blackman-Harris, 3 = rectangular,
 *                as dp_psd_create().
 * @param beta    Kaiser beta (ignored for the other windows).
 * @param mode    DP_SPECTROGRAM_DB. DP_SPECTROGRAM_POWER is refused for now.
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
 * dp_spectrogram_consumed() and dp_spectrogram_pending() read 0.
 * Configuration is kept.
 *
 * @param s  Must be non-NULL.
 */
void dp_spectrogram_reset (dp_spectrogram_state_t *s);

/**
 * @brief Push input, write every whole row it completes that fits.
 *
 * Takes the input in order and writes row after row into @p out, each
 * @c nfft floats, until either the input is used up or the next row is due
 * and @p out has no room for it. Only whole rows are written: a @p max_out
 * that is not a multiple of nfft uses floor(max_out / nfft) rows of it, and
 * an @p out too small for one row writes nothing.
 *
 * A short @p out never loses input. A sample is taken unless taking it would
 * complete a row @p out has no room for: the framer's feed contract
 * (DECLARE_DP_BUFFER_FRAMES) applied to rows. dp_spectrogram_consumed()
 * reports how many were taken, and the caller offers the rest again. Taken
 * input that does not yet complete a row is the carry, held inside: fewer
 * than nfft samples once this returns. So input that completes no row is
 * always taken whole, even with @p max_out 0.
 *
 * @param s        Must be non-NULL.
 * @param in       Complex baseband samples (cf32).
 * @param n_in     Samples in @p in.
 * @param out      Rows, row-major, nfft floats each.
 * @param max_out  Floats @p out has room for.
 * @return Floats written: a multiple of nfft, at most
 *         dp_spectrogram_push_max_out(s, n_in).
 *
 * @code
 * // nfft 8, hop 4, rectangular, dB: a unit tone on bin 2
 * dp_spectrogram_state_t *s = dp_spectrogram_create (8, 4, 3, 0.0f, 0);
 * float _Complex x[16];
 * for (int i = 0; i < 16; i++)
 *   x[i] = cexpf (I * 2.0f * 3.14159265f * 2.0f * (float)i / 8.0f);
 * float rows[3 * 8];
 * size_t got = dp_spectrogram_push (s, x, 16, rows, 3 * 8);
 * // 16 samples at hop 4 complete the rows starting at 0, 4 and 8
 * if (got != 3 * 8 || dp_spectrogram_consumed (s) != 16)
 *   return 1;
 * if (fabsf (rows[4 + 2]) > 1e-4f)   // bin 2 reads 0 dBFS, at nfft/2 + 2
 *   return 1;
 * dp_spectrogram_destroy (s);
 * @endcode
 */
size_t dp_spectrogram_push (dp_spectrogram_state_t *s,
                            const float _Complex *in, size_t n_in, float *out,
                            size_t max_out);

/**
 * @brief Floats one push of @p n_in samples writes when @p out has room:
 *        dp_spectrogram_rows_for(s, n_in) * nfft.
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
 * @param s    Must be non-NULL.
 * @param row  Room for nfft floats.
 * @return Floats written: nfft, or 0 if no row was owed.
 */
size_t dp_spectrogram_flush (dp_spectrogram_state_t *s, float *row);

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
 * The blob is the stream position (the framer's snapshot of the carry)
 * inside the spectrogram's own envelope, and nothing else: not consumed(),
 * and not the configuration. So a blob restores into a spectrogram created
 * with the SAME arguments as the one it came from, and keeping them the same
 * is the caller's -- see dp_spectrogram_set_state().
 *
 * @param s  Must be non-NULL.
 */
size_t dp_spectrogram_state_bytes (const dp_spectrogram_state_t *s);

/**
 * @brief Serialize the stream position into @p blob.
 * @param s     Must be non-NULL.
 * @param blob  dp_spectrogram_state_bytes(s) bytes, every one written.
 */
void dp_spectrogram_get_state (const dp_spectrogram_state_t *s, void *blob);

/**
 * @brief Restore a stream position, so the next push continues it bit for bit.
 *
 * Precondition, the caller's to keep because the blob does not carry it:
 * @p s was created with the same arguments (nfft, hop, window, beta, mode)
 * as the spectrogram the blob came from. A different nfft or hop is refused
 * (the size and the framer's stored hop tell); a different window or beta is
 * NOT, and the rows that follow are that window's, not the original's.
 *
 * @param s     A spectrogram created with the same arguments.
 * @param blob  From dp_spectrogram_get_state().
 * @return DP_OK, or DP_ERR_INVALID (wrong magic, version, size or hop, or a
 *         corrupt carry), with @p s unchanged.
 */
int dp_spectrogram_set_state (dp_spectrogram_state_t *s, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DP_SPECTROGRAM_CORE_H */
