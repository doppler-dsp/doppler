/**
 * @file farrow_core.h
 * @brief Farrow fractional-delay interpolator — linear / parabolic / cubic.
 *
 * A selectable-order Lagrange interpolator in Farrow (Horner-in-µ) form — the
 * lean alternative to a full polyphase resampler when all you need is a
 * fractional-delay tap for a timing loop. All three orders share one 4-tap
 * delay line and interpolate at the SAME point — between the two middle taps —
 * so the **group delay is 2 samples regardless of order**, which keeps a driving
 * symbol-timing loop order-agnostic. Push input samples with farrow_push();
 * evaluate the output at a fractional offset µ ∈ `[0,1)` with farrow_eval(). The
 * fractional offset is meant to come from an integer timing NCO (the post-wrap
 * accumulator value), so the timing stays drift-free while only the
 * interpolation itself is floating point.
 *
 * order: 0 = linear (2-tap Lagrange), 1 = parabolic (4-tap symmetric
 *        piecewise-parabolic Farrow, α = 0.5), 2 = cubic (4-tap cubic Lagrange).
 *        All three are symmetric about the interpolation point, so the phase
 *        (delay) response is linear — no timing bias. Linear and cubic are
 *        exact for degree 1 and 3 polynomials; the piecewise-parabolic trades
 *        exactness for a flatter magnitude response than linear at no delay
 *        cost.
 *
 * Lifecycle: farrow_create -> (push / eval / reset)* -> farrow_destroy, or embed
 * by value with farrow_init().
 *
 * @code
 * farrow_state_t f;
 * farrow_init(&f, FARROW_CUBIC);
 * for (size_t i = 0; i < n; i++) farrow_push(&f, x[i]);
 * float complex y = farrow_eval(&f, 0.3f);   // x interpolated 0.3 past tap[1]
 * @endcode
 */
#ifndef FARROW_CORE_H
#define FARROW_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "dp_state.h"
#include <complex.h>
#ifdef __cplusplus
extern "C" {
#endif

enum {
    FARROW_LINEAR = 0,
    FARROW_PARABOLIC = 1,
    FARROW_CUBIC = 2,
};

/* All orders interpolate between tap d[1] and d[2]; d[3] is the newest sample,
 * so the interpolation point lags the newest input by FARROW_GROUP_DELAY. */
#define FARROW_GROUP_DELAY 2u

/**
 * @brief Farrow interpolator state (4-tap delay line + order).
 *
 * Public so a timing loop can embed it by value; treat the delay line as
 * internal (drive it through farrow_push / farrow_eval).
 */
typedef struct {
    float complex d[4]; /**< delay line, `d[3]` newest.                */
    int order;          /**< FARROW_LINEAR / _PARABOLIC / _CUBIC.      */
} farrow_state_t;

/** @brief Initialise in place: set order, clear the delay line. */
JM_FORCEINLINE void
farrow_init (farrow_state_t *s, int order)
{
    s->d[0] = s->d[1] = s->d[2] = s->d[3] = 0.0f;
    s->order = order;
}

/** @brief Push one input sample into the delay line (oldest drops out). */
JM_FORCEINLINE JM_HOT void
farrow_push (farrow_state_t *s, float complex x)
{
    s->d[0] = s->d[1];
    s->d[1] = s->d[2];
    s->d[2] = s->d[3];
    s->d[3] = x;
}

/**
 * @brief Interpolate at fractional offset @p mu ∈ `[0,1)` between `d[1]` and `d[2]`.
 *
 * Horner-in-µ evaluation of the order's Lagrange polynomial. µ = 0 returns
 * `d[1]` (= input at i - 2); µ → 1 returns `d[2]`.
 *
 * @param s   State.  Must be non-NULL.
 * @param mu  Fractional offset in `[0,1)`.
 * @return The interpolated sample.
 */
JM_FORCEINLINE JM_HOT float complex
farrow_eval (const farrow_state_t *s, float mu)
{
    float complex d0 = s->d[0], d1 = s->d[1], d2 = s->d[2], d3 = s->d[3];
    if (s->order == FARROW_LINEAR)
        return d1 + mu * (d2 - d1);
    if (s->order == FARROW_PARABOLIC)
        {
            /* symmetric piecewise-parabolic (Farrow, α = 0.5): linear plus a
             * symmetric μ(μ-1) parabolic correction → no delay bias. */
            float complex u  = d0 - d1 - d2 + d3;
            float complex c2 = 0.5f * u;
            float complex c1 = (d2 - d1) - c2;
            return (c2 * mu + c1) * mu + d1;
        }
    /* cubic */
    float complex c3 = (1.0f / 6.0f) * (-d0 + 3.0f * d1 - 3.0f * d2 + d3);
    float complex c2 = 0.5f * (d0 - 2.0f * d1 + d2);
    float complex c1
        = (1.0f / 6.0f) * (-2.0f * d0 - 3.0f * d1 + 6.0f * d2 - d3);
    return ((c3 * mu + c2) * mu + c1) * mu + d1;
}

/**
 * @brief Create a Farrow interpolator.
 * @param order  0 = linear, 1 = parabolic, 2 = cubic.
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call farrow_destroy() when done.
 */
farrow_state_t *farrow_create(int order);

/** @brief Destroy a Farrow interpolator.  @param state May be NULL. */
void farrow_destroy(farrow_state_t *state);

/**
 * @brief Clear the interpolator delay line; keep the order.
 *
 * Zeroes the 4-tap delay line so the next block starts from a filling
 * transient again, exactly as a freshly created interpolator would. The
 * order (linear / parabolic / cubic) is preserved, so the same object can be
 * reused across independent bursts without rebuilding the polynomial. Call it
 * between unrelated signal segments to stop the tail of one leaking into the
 * head of the next.
 *
 * @param state  Must be non-NULL.
 *
 * @code
 * >>> from doppler.resample import Farrow
 * >>> import numpy as np
 * >>> f = Farrow(order="cubic")
 * >>> _ = f.delay(np.ones(8, dtype=np.complex64), 0.25)  # leaves state
 * >>> f.reset()                                          # back to pristine
 * >>> x = np.arange(8, dtype=np.complex64)
 * >>> f.delay(x, 0.5)[3:].real.tolist()   # steady part == ramp shifted 1.5
 * [1.5, 2.5, 3.5, 4.5, 5.5]
 *
 * @endcode
 */
void farrow_reset(farrow_state_t *state);

size_t farrow_delay_max_out(farrow_state_t *state);
/**
 * @brief Apply a constant fractional delay of @p mu samples to a CF32 block.
 *
 * Pushes each input sample through the delay line and evaluates the
 * interpolator at the same fixed offset, so the whole block is delayed by a
 * constant, non-integer amount. Output sample i is the input interpolated at
 * `i - group_delay + mu`, i.e. the stream shifted later by
 * `group_delay - mu` samples; the first @c group_delay outputs are the
 * delay-line filling transient and should be discarded. Because the offset is
 * held constant this is the open-loop use of the interpolator — a timing loop
 * instead steers @c mu per sample via farrow_push()/farrow_eval().
 *
 * @param state    Pointer to a valid farrow_state_t.
 * @param x        CF32 input samples.
 * @param x_len    Number of input samples.
 * @param mu       Fractional delay in samples; the offset in `[0,1)` into the
 *                 interpolation interval (values outside extrapolate).
 * @param out      Output buffer; one output per input sample.
 * @param max_out  Capacity of @p out in samples.
 * @return CF32 output array, same length as @p x, each sample delayed by
 *         `group_delay - mu`.
 *
 * @code
 * >>> from doppler.resample import Farrow
 * >>> import numpy as np
 * >>> f = Farrow(order="cubic")
 * >>> x = np.arange(8, dtype=np.complex64)   # a ramp: exactly interpolable
 * >>> y = f.delay(x, 0.5)                     # delay by group_delay - 0.5
 * >>> [round(float(v.real), 4) for v in y]    # first 2 are fill transient
 * [0.0, -0.0625, 0.4375, 1.5, 2.5, 3.5, 4.5, 5.5]
 *
 * @endcode
 */
size_t farrow_delay(farrow_state_t *state, const float complex *x, size_t x_len, double mu, float complex *out, size_t max_out);
size_t farrow_get_group_delay(const farrow_state_t *state);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Whole-struct POD snapshot (pointer-free); the 4-tap delay line + order resume exactly into an
 * identically-built instance. */
#define FARROW_STATE_MAGIC DP_FOURCC ('F', 'R', 'R', 'W')
#define FARROW_STATE_VERSION 1u
size_t farrow_state_bytes (const farrow_state_t *state);
void   farrow_get_state (const farrow_state_t *state, void *blob);
int    farrow_set_state (farrow_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* FARROW_CORE_H */
