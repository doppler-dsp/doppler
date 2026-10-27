/**
 * @file acc_trace_core.h
 * @brief AccTrace — per-bin vector trace accumulator.
 *
 * Folds a stream of equal-length frames into a single running trace using one
 * of four reduction modes: linear mean, exponential moving average (EMA),
 * max-hold, or min-hold.  Where the scalar @c acc_f32 / @c acc_cf64
 * accumulators reduce a frame to one running sum, AccTrace keeps a value
 * *per bin*, which is what spectrum averaging,
 * waterfalls/spectrograms, and video-averaged displays need.  Accumulation is
 * done in double precision regardless of the float32 input/output to keep the
 * running mean numerically stable over long captures.
 *
 * The first frame seeds the trace in every mode; subsequent frames update it:
 *   - mean    : ``acc += (p - acc) / count``      (Welford running mean)
 *   - exp     : ``acc  = alpha*p + (1-alpha)*acc`` (EMA)
 *   - maxhold : ``acc  = max(acc, p)``  per bin
 *   - minhold : ``acc  = min(acc, p)``  per bin
 *
 * Lifecycle: create -> (accumulate / reset)* -> value -> destroy
 */
#ifndef DP_ACC_TRACE_CORE_H
#define DP_ACC_TRACE_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/dp_state.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Trace reduction mode. The Python faces (AccTrace, PSD) name these
 * "mean", "exp", "maxhold" and "minhold", bound to the constants by name
 * (just-makeit.toml, `[[enum]] acc_trace_mode`).
 */
/* SSOT: enum=acc_trace_mode */
typedef enum {
    ACC_TRACE_MEAN = 0,    /**< Linear running mean of all frames.   */
    ACC_TRACE_EXP = 1,     /**< Exponential moving average (alpha).   */
    ACC_TRACE_MAXHOLD = 2, /**< Per-bin running maximum.              */
    ACC_TRACE_MINHOLD = 3, /**< Per-bin running minimum.              */
} acc_trace_mode_t;

/**
 * @brief AccTrace state.  Allocate with dp_acc_trace_create().
 */
typedef struct {
    double *acc;            /**< Running trace, length n (double). */
    size_t n;               /**< Trace length (bins).             */
    acc_trace_mode_t mode;  /**< Reduction mode.                  */
    double alpha;           /**< EMA smoothing factor (exp mode). */
    uint64_t count;         /**< Frames folded in so far.         */
} dp_acc_trace_state_t;

/**
 * @brief Create a length-@p n trace accumulator.
 *
 * @param n      Trace length in bins.  Must be > 0, with n doubles a
 *               byte count a size_t holds; returns NULL otherwise.
 * @param mode   Reduction mode: "mean", "exp", "maxhold" or "minhold".
 * @param alpha  EMA smoothing factor, read only by @c exp mode, where it must
 *               lie in (0, 1].  Outside it the trace is not an average: 0
 *               never leaves the first frame, a negative alpha extrapolates
 *               away from the data (a power trace goes negative), and above
 *               1 the EMA step saturates to pass-through.  A NaN is refused
 *               too.  Ignored, and so not checked, in the other modes.
 * @return Heap-allocated state, or NULL on invalid argument (including an
 *         exp-mode @p alpha outside (0, 1]) or OOM.
 * @note Caller must call dp_acc_trace_destroy() when done.
 *
 * @code
 * >>> from doppler.accumulator import AccTrace
 * >>> acc = AccTrace(n=8, mode="mean")
 * >>> acc.n, acc.count
 * (8, 0)
 * @endcode
 */
dp_acc_trace_state_t *dp_acc_trace_create(size_t n, int mode, double alpha);

/**
 * @brief Set the EMA smoothing factor, under dp_acc_trace_create()'s rule.
 *
 * In @c exp mode an @p alpha outside (0, 1], or a NaN, is refused and the
 * trace keeps the alpha it had; the other modes never read it and accept any
 * value, as create() does.  The running trace is not touched either way.
 *
 * @param state  Must be non-NULL.
 * @param alpha  New smoothing factor.
 * @return DP_OK, or DP_ERR_INVALID when refused (state unchanged).
 */
int dp_acc_trace_set_alpha(dp_acc_trace_state_t *state, double alpha);

/**
 * @brief Destroy an AccTrace instance and release all memory.
 * @param state  May be NULL (no-op).
 */
void dp_acc_trace_destroy(dp_acc_trace_state_t *state);

/**
 * @brief Discard the running trace; the next accumulate re-seeds it.
 * The mode, alpha, and length are preserved.
 * @param state  Must be non-NULL.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.accumulator import AccTrace
 * >>> acc = AccTrace(n=4, mode="mean")
 * >>> acc.accumulate(np.ones(4, dtype=np.float32))
 * >>> acc.reset()
 * >>> acc.count
 * 0
 * @endcode
 */
void dp_acc_trace_reset(dp_acc_trace_state_t *state);

/**
 * @brief Fold one length-n frame into the running trace.
 * Frames shorter than @p n are ignored; if @p p_len exceeds @p n only the
 * first @p n samples are used.  The first accumulated frame seeds the trace
 * directly (every mode), so a single frame followed by value() returns that
 * frame unchanged.
 *
 * @param state  Must be non-NULL.
 * @param p      Input frame (float32).
 * @param p_len  Number of samples in @p p; must be >= n to take effect.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.accumulator import AccTrace
 * >>> acc = AccTrace(n=4, mode="mean")
 * >>> acc.accumulate(np.array([1, 3, 5, 7], dtype=np.float32))
 * >>> acc.accumulate(np.array([3, 5, 7, 9], dtype=np.float32))
 * >>> acc.value().tolist()
 * [2.0, 4.0, 6.0, 8.0]
 * @endcode
 */
void dp_acc_trace_accumulate(dp_acc_trace_state_t *state, const float *p,
                          size_t p_len);

/**
 * @brief Output capacity value() needs: the trace length n.
 * @param state  Must be non-NULL.
 * @return The trace length n (bins) given to create(); constant.
 */
size_t dp_acc_trace_value_max_out(dp_acc_trace_state_t *state);

/**
 * @brief Return the current reduced trace, one value per bin.
 * Copies the full length-n trace out to the caller and returns the number of
 * samples written; the Python wrapper turns that into a fresh float32 array of
 * the reduced trace (the running mean, EMA, max-hold, or min-hold, per the
 * construction @c mode).  Before any frame has been accumulated the trace is
 * empty: the count is 0, which the wrapper renders as ``None``.  Reading is
 * non-destructive — the running trace keeps accumulating across later frames.
 *
 * @param state  Must be non-NULL.
 * @param n      Caller buffer capacity (ignored; buffer is pre-sized to n).
 * @param out    Destination, at least n float32 elements.
 * @param max_out Capacity of @p out in elements. Emission stops there, so the
 *               return value is the number actually written.
 * @return min(trace length, @p max_out) samples written, or 0 before the
 *         first frame (or after reset()).
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.accumulator import AccTrace
 * >>> acc = AccTrace(n=3, mode="maxhold")
 * >>> acc.value() is None            # empty until the first frame
 * True
 * >>> acc.accumulate(np.array([1, 5, 2], dtype=np.float32))
 * >>> acc.accumulate(np.array([4, 3, 6], dtype=np.float32))
 * >>> acc.value().tolist()           # per-bin running maximum
 * [4.0, 5.0, 6.0]
 *
 * @endcode
 */
size_t dp_acc_trace_value(dp_acc_trace_state_t *state, size_t n, float *out,
                       size_t max_out);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Field-wise: [u32 mode][u64 count][f64 alpha][running trace]; n is restored
 * by create.  mode is a reject key: a blob from a trace in another mode is
 * refused.  alpha travels because dp_acc_trace_set_alpha() can change it after
 * create (version 2; version 1 blobs, without mode or alpha, are refused).
 * set_state refuses an alpha the setter would refuse; either refusal leaves
 * the state untouched. */
/** @brief State-blob magic ('ATRC'); set_state() rejects any other. */
#define ACC_TRACE_STATE_MAGIC DP_FOURCC ('A','T','R','C')
/** @brief State-blob layout version (2: carries mode and alpha). */
#define ACC_TRACE_STATE_VERSION 2u
/**
 * @brief Size in bytes of this trace's state blob.
 * @param state  Must be non-NULL.
 * @return Header + mode (u32) + count (u64) + alpha (f64) + n doubles.
 */
size_t dp_acc_trace_state_bytes (const dp_acc_trace_state_t *state);
/**
 * @brief Serialize mode, count, alpha and the running trace into @p blob.
 * @param state  Must be non-NULL.
 * @param blob   Destination; at least dp_acc_trace_state_bytes() bytes.
 */
void dp_acc_trace_get_state (const dp_acc_trace_state_t *state, void *blob);
/**
 * @brief Restore a trace from a blob made by dp_acc_trace_get_state().
 * @param state  Must be non-NULL, created with the same n.
 * @param blob   Source blob of dp_acc_trace_state_bytes() bytes.
 * @return `DP_OK`, or `DP_ERR_INVALID` (state untouched) for a header
 *         mismatch, a blob from another mode, or an alpha the setter would
 *         refuse.
 */
int dp_acc_trace_set_state (dp_acc_trace_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* ACC_TRACE_CORE_H */
