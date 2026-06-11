/**
 * @file acc_trace_core.h
 * @brief AccTrace — per-bin vector trace accumulator.
 *
 * Folds a stream of equal-length frames into a single running trace using one
 * of four reduction modes: linear mean, exponential moving average (EMA),
 * max-hold, or min-hold.  Where ::acc_f32 reduces a frame to one scalar sum,
 * AccTrace keeps a value *per bin*, which is what spectrum averaging,
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
#ifndef ACC_TRACE_CORE_H
#define ACC_TRACE_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Trace reduction mode.  Values match the Python string-enum index
 * order ("mean", "exp", "maxhold", "minhold").
 */
typedef enum {
    ACC_TRACE_MEAN = 0,    /**< Linear running mean of all frames.   */
    ACC_TRACE_EXP = 1,     /**< Exponential moving average (alpha).   */
    ACC_TRACE_MAXHOLD = 2, /**< Per-bin running maximum.              */
    ACC_TRACE_MINHOLD = 3, /**< Per-bin running minimum.              */
} acc_trace_mode_t;

/**
 * @brief AccTrace state.  Allocate with acc_trace_create().
 */
typedef struct {
    double *acc;            /**< Running trace, length n (double). */
    size_t n;               /**< Trace length (bins).             */
    acc_trace_mode_t mode;  /**< Reduction mode.                  */
    double alpha;           /**< EMA smoothing factor (exp mode). */
    uint64_t count;         /**< Frames folded in so far.         */
} acc_trace_state_t;

/**
 * @brief Create a length-@p n trace accumulator.
 *
 * @param n      Trace length in bins.  Must be > 0; returns NULL otherwise.
 * @param mode   Reduction mode index (0=mean, 1=exp, 2=maxhold, 3=minhold).
 * @param alpha  EMA smoothing factor used only by @c exp mode (0 < alpha <= 1).
 * @return Heap-allocated state, or NULL on invalid argument or OOM.
 * @note Caller must call acc_trace_destroy() when done.
 *
 * @code
 * >>> from doppler.accumulator import AccTrace
 * >>> acc = AccTrace(n=8, mode="mean")
 * >>> acc.n, acc.count
 * (8, 0)
 * @endcode
 */
acc_trace_state_t *acc_trace_create(size_t n, int mode, double alpha);

/**
 * @brief Destroy an AccTrace instance and release all memory.
 * @param state  May be NULL (no-op).
 */
void acc_trace_destroy(acc_trace_state_t *state);

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
void acc_trace_reset(acc_trace_state_t *state);

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
void acc_trace_accumulate(acc_trace_state_t *state, const float *p,
                          size_t p_len);

/** @brief Output capacity hint for value(); equals the trace length n. */
size_t acc_trace_value_max_out(acc_trace_state_t *state);

/**
 * @brief Copy the current averaged trace into @p out.
 * Writes the full length-n trace and returns n.  Returns 0 (which the Python
 * wrapper renders as None) before any frame has been accumulated.
 *
 * @param state  Must be non-NULL.
 * @param n      Caller buffer capacity (ignored; buffer is pre-sized to n).
 * @param out    Destination, at least n float32 elements.
 * @return Number of samples written (n, or 0 if empty).
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.accumulator import AccTrace
 * >>> acc = AccTrace(n=3, mode="maxhold")
 * >>> acc.accumulate(np.array([1, 5, 2], dtype=np.float32))
 * >>> acc.accumulate(np.array([4, 3, 6], dtype=np.float32))
 * >>> acc.value().tolist()
 * [4.0, 5.0, 6.0]
 * @endcode
 */
size_t acc_trace_value(acc_trace_state_t *state, size_t n, float *out);
#ifdef __cplusplus
}
#endif

#endif /* ACC_TRACE_CORE_H */
