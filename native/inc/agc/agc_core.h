/**
 * @file agc_core.h
 * @brief Log-domain automatic gain control (AGC).
 *
 * A feedback AGC that drives the average power of its output toward a
 * fixed reference level.  Three stages run per sample:
 *
 *   1. Gain        y = x * 10^(gain_db / 20)
 *   2. Detector    p_avg += alpha * (|y|^2 - p_avg)
 *   3. Loop filter gain_db += (4*loop_bw) * (ref_db - 10*log10(p_avg))
 *
 * @par Loop bandwidth
 * The loop filter is a single integrator whose step size is @c 4*loop_bw.
 * @c loop_bw is the loop's normalised noise-equivalent bandwidth in
 * cycles/sample: a 1st-order loop with integrator step @c mu has a noise
 * bandwidth of @c mu/4, so the knob is expressed as a bandwidth rather
 * than a bare loop gain.  Smaller @c loop_bw is slower and smoother.
 *
 * @par Linear-in-dB
 * The control variable @c gain_db and the detector output are both in
 * decibels, so the closed loop is a linear 1st-order recursion in the
 * dB domain.  Because output power (dB) equals input power (dB) plus
 * @c gain_db, the loop reduces to
 *
 * @code
 *   gain_db[n+1] = (1 - 4*loop_bw) * gain_db[n]
 *                + (4*loop_bw) * (ref_db - px_db[n])
 * @endcode
 *
 * which converges to @c gain_db = ref_db - px_db with a time constant of
 * roughly @c 1/(4*loop_bw) samples — independent of the absolute signal
 * level.  A 60 dB-loud signal and a 0 dB-quiet signal settle in the same
 * number of samples; only a level-dependent loop would not.
 *
 * @par Power detector
 * @c p_avg is an exponential moving average (1-pole leaky integrator) of
 * the instantaneous output power @c |y|^2.  @c alpha in (0, 1] sets the
 * detector bandwidth: small @c alpha smooths hard but reacts slowly.
 *
 * @par Topology
 * Feedback — power is measured AFTER the gain.  The gain applied to
 * sample @c n is computed from samples up to @c n-1, so the per-sample
 * loop is inherently sequential.
 *
 * @par Block processing
 * agc_step() advances the control loop every sample.  agc_steps()
 * decimates it: the detector + loop filter run once per chunk of
 * @c decim samples (default @c AGC_DECIM_DEFAULT; typically 8, 16 or
 * 32).  The gain the loop commands is linearly interpolated across the
 * chunk — a first-order hold, so the applied gain has no inter-chunk
 * staircase — while the gain-apply and the power sum vectorise.  This is
 * sound because the detector average already band-limits the envelope,
 * but it makes agc_steps() not bit-identical to a per-sample agc_step()
 * loop, only equivalent at convergence.  The per-block detector and loop
 * coefficients are rescaled from @c alpha / @c loop_bw internally, so
 * those keep their per-sample meaning; keep @c loop_bw well below
 * @c 1/(4*decim) for loop stability.
 *
 * @par Output clipping
 * Each output sample is square-clipped: the real and imaginary parts
 * are independently limited to @c +/-10^(clip_db/20) — a square region
 * in the IQ plane, not a circular magnitude limit.  The clip is the
 * last operation applied to the output and does NOT feed the power
 * detector: the loop always measures the true, unclipped power, so
 * clipping never disturbs convergence.  @c clip_db defaults to
 * @c AGC_CLIP_DB_DEFAULT, which is high enough to be effectively off.
 *
 * Lifecycle: `agc_create -> (step / steps / reset)* -> agc_destroy`
 *
 * @code
 * // Hold output power at 0 dB; slow loop, moderate detector smoothing.
 * agc_state_t *agc = agc_create(0.0, 0.0025, 0.05);
 * float complex y = agc_step(agc, 4.0f + 0.0f * I);  // loud input
 * // ... feed more samples; gain_db converges so 10*log10(|y|^2) -> 0 dB
 * agc_destroy(agc);
 * @endcode
 */
#ifndef AGC_CORE_H
#define AGC_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "dp_state.h"
#include "util/util_core.h"
#include <math.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Power floor for the detector, in linear units.
 *
 * Substituted for @c p_avg inside @c log10() so that a long run of
 * silence yields a large-but-finite measured level (about -300 dB)
 * instead of @c -INF / @c NaN.  Also keeps the log10() argument a normal
 * (non-denormal) double.  Never reached in normal operation — @c p_avg
 * is seeded with the reference power at create/reset.
 */
#define AGC_POWER_FLOOR 1e-30

/**
 * @brief Default envelope decimation factor (agc_state_t::decim).
 *
 * agc_steps() runs the detector + loop filter once per chunk of
 * @c decim samples.  @c decim must stay small relative to the loop time
 * constant ~1/(4*loop_bw); useful values are 8, 16 and 32.  8 keeps the
 * gain trajectory well inside the default loop bandwidth and is one
 * AVX-width vector for the in-chunk gain-apply.
 */
#define AGC_DECIM_DEFAULT 8

/**
 * @brief Default output clip level (agc_state_t::clip_db), in dB.
 *
 * 120 dB is a per-component amplitude limit of 10^6 — far above any
 * normally scaled signal, so output clipping is effectively disabled
 * until @c clip_db is lowered.  See agc_state_t::clip_db.
 */
#define AGC_CLIP_DB_DEFAULT 120.0

  /**
   * @brief Fast 10^v approximation (~1e-3 relative).
   *
   * Routes through 2^z = 2^floor(z) * 2^frac with z = v*log2(10): the
   * integer part becomes a raw IEEE-754 exponent and the fractional part
   * a 4th-order Taylor series.  Far cheaper than libm pow(); the AGC loop
   * tolerates orders of magnitude more error than this.
   */
  JM_FORCEINLINE double
  agc_exp10_ (double v)
  {
    double z = v * 3.321928094887362; /* z = v * log2(10)        */
    double zi = floor (z);
    double u = (z - zi) * 0.6931471805599453; /* frac(z) * ln2, [0, ln2) */
    /* 2^frac = e^u via 4th-order Taylor: 1 + u + u^2/2 + u^3/6 + u^4/24. */
    double f = 1.0
               + u
                     * (1.0
                        + u
                              * (0.5
                                 + u
                                       * (0.16666666666666666
                                          + u * 0.041666666666666664)));
    /* 2^floor(z): assemble the exponent field directly. */
    uint64_t bits = (uint64_t)((int64_t)zi + 1023) << 52;
    double pow2i;
    memcpy (&pow2i, &bits, sizeof pow2i);
    return pow2i * f;
  }

  /**
   * @brief Fast log10(p) approximation for p > 0 (~1e-3 absolute).
   *
   * Splits p = m * 2^e via the IEEE-754 fields, takes log2(m) from the
   * atanh series with t = (m-1)/(m+1) in &#91;0, 1/3&#93; (two terms), and scales
   * log2 by log10(2).  Used only on the decimated control path, so even
   * the divide is amortised across a decimation chunk.
   */
  JM_FORCEINLINE double
  agc_log10_ (double p)
  {
    uint64_t bits;
    memcpy (&bits, &p, sizeof bits);
    int e = (int)((bits >> 52) & 0x7FF) - 1023; /* p = m * 2^e       */
    bits = (bits & 0x000FFFFFFFFFFFFFULL) | 0x3FF0000000000000ULL;
    double m;
    memcpy (&m, &bits, sizeof m); /* m in [1, 2)       */
    /* log2(m) = (2/ln2) * (t + t^3/3 + ...), t = (m-1)/(m+1) in [0,1/3]. */
    double t = (m - 1.0) / (m + 1.0);
    double log2m = 2.885390081777927 * t * (1.0 + t * t * 0.3333333333333333);
    return ((double)e + log2m) * 0.30102999566398120; /* * log10(2)      */
  }

  /**
   * @brief Power |y|^2 in the detector's working precision (double).
   *
   * The power detector EMA, the dB loop filter and @c agc_log10_ all work in
   * double across the AGC's full (dB) dynamic range, so the squaring promotes
   * the float components once.  Defined here so agc_step() — and any composing
   * sample loop that accumulates AGC input power — measures power identically.
   */
  JM_FORCEINLINE double
  agc_power_ (float complex y)
  {
    double yr = (double)crealf (y), yi = (double)cimagf (y);
    return yr * yr + yi * yi;
  }

  /**
   * @brief AGC state.
   *
   * Allocate with agc_create().  @c ref_db, @c loop_bw, @c alpha,
   * @c decim, @c clip_db and @c gain_update_period are configuration
   * (readable and writable at runtime); @c gain_db, @c p_avg, @c g_last,
   * @c gain_phase and @c clip_lin are the loop's internal memory.
   */
  typedef struct
  {
    double ref_db;  /* target output power, dB                        */
    double loop_bw; /* loop noise bandwidth, cycles/sample             */
    double alpha;   /* power-detector EMA coefficient, (0, 1]          */
    size_t decim;   /* agc_steps() chunk length (8 / 16 / 32)          */
    double clip_db; /* output square-clip level, dB (per component)    */
    /* agc_step() control-update period: the detector + gain-apply run
     * every sample, but the loop-filter command (the exp10/log10 work)
     * refreshes once per this many samples — a zero-order hold on the
     * gain that amortises the transcendentals on a sample-rate hot loop.
     * 1 (default) is the exact per-sample loop; >1 trades gain-update
     * latency for speed (keep well below 1/(4*loop_bw), like decim). */
    size_t gain_update_period;
    double gain_db; /* loop-filter integrator: current gain, dB        */
    double p_avg;   /* power-detector EMA: averaged output power, lin  */
    double g_last;  /* current linear gain held across the period      */
    size_t gain_phase; /* agc_step() position in the update period     */
    float  clip_lin;   /* cached 10^(clip_db/20), refreshed per period */
  } agc_state_t;

  /**
   * @brief Construct a log-domain feedback AGC and return its heap state.
   * The loop integrator starts at 0 dB (unity gain) and the power detector
   * @c p_avg is pre-seeded to @c 10^(ref_db/10) linear, so the first block
   * of on-target samples produces no transient.  Three parameters tune the
   * closed-loop behaviour: @p ref_db sets the target, @p loop_bw sets the
   * convergence speed, and @p alpha sets the detector smoothing.
   * @param ref_db   Target output power in dB (e.g. @c 0.0 for unity power).
   * @param loop_bw  Loop noise bandwidth in cycles/sample; the loop settles
   *                 in roughly @c 1/(4*loop_bw) samples.  Smaller values
   *                 are slower and smoother; keep well below
   *                 @c 1/(4*decim) when using agc_steps().
   * @param alpha    Power-detector EMA coefficient in (0, 1]; smaller values
   *                 smooth harder but react slower to envelope changes.
   * @return Heap-allocated @c agc_state_t, or @c NULL on allocation failure.
   *         The caller must call agc_destroy() when done.
   * @code
   * >>> from doppler.agc import AGC
   * >>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
   * >>> agc.ref_db, agc.loop_bw, agc.alpha
   * (0.0, 0.0025, 0.05)
   * >>> agc.gain_db, agc.applied_gain_db
   * (0.0, 0.0)
   * >>> agc.decim, agc.clip_db
   * (8, 120.0)
   * @endcode
   */
agc_state_t *agc_create(double ref_db, double loop_bw, double alpha);

  /**
   * @brief Destroy an AGC instance and release all memory.
   * Frees the heap-allocated @c agc_state_t.  Safe to call with @c NULL.
   * After this call the pointer is invalid; set it to @c NULL.  The
   * Python binding calls this automatically when the object is garbage-
   * collected or when used as a context manager (@c with AGC() as agc:).
   * @param state  Pointer to the state to free; may be @c NULL (no-op).
   * @code
   * >>> from doppler.agc import AGC
   * >>> agc = AGC()
   * >>> agc.destroy()   # explicit release
   * >>> with AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05) as agc2:
   * ...     y = agc2.step(1.0+0.0j)
   * ...     y
   * (1+0j)
   * @endcode
   */
void agc_destroy(agc_state_t *state);

  /**
   * @brief Reset the AGC loop state to its post-create condition.
   * Sets @c gain_db back to 0 dB (unity), clears @c g_last, and re-seeds
   * the power-detector EMA @c p_avg from the current @c ref_db so that
   * the first post-reset block produces no transient.  All configuration
   * fields (@c ref_db, @c loop_bw, @c alpha, @c decim, @c clip_db) are
   * left untouched.  Use this to process a new, independent signal segment
   * without re-allocating.
   * @param state  Must be non-NULL.
   * @code
   * >>> from doppler.agc import AGC
   * >>> import numpy as np
   * >>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
   * >>> _ = agc.steps(np.full(1000, 4.0+0.0j, dtype=np.complex64))
   * >>> round(agc.gain_db, 1)   # converged to -12 dB
   * -12.0
   * >>> agc.reset()
   * >>> agc.gain_db, agc.applied_gain_db
   * (0.0, 0.0)
   * @endcode
   */
void agc_reset(agc_state_t *state);

  /**
   * @brief Process one complex sample through the per-sample AGC loop.
   * Applies the current gain, measures the output power via the EMA detector,
   * advances the loop-filter integrator, then square-clips the returned sample
   * to @c clip_db.  The clip is applied after the detector update, so clipping
   * never disturbs convergence.  With the default @c gain_update_period == 1
   * this is the exact per-sample reference path; with @c gain_update_period
   * P > 1 the detector and gain-apply still run every sample but the loop-filter
   * command (and the exp10/log10 it needs) refreshes once per P samples — a
   * zero-order hold on the gain that amortises the transcendentals on a
   * sample-rate hot loop, the streaming analogue of agc_steps()' decimation.
   * agc_steps() is the faster block equivalent; neither is bit-identical to the
   * P == 1 loop once decimated, but both converge to the same steady state.
   * @param state  Must be non-NULL.
   * @param x      Complex input sample.
   * @return       Gained, clipped output sample @c x * 10^(gain_db/20) with
   *               each component independently clamped to
   *               @c +/-10^(clip_db/20).
   * @code
   * >>> from doppler.agc import AGC
   * >>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
   * >>> agc.step(1.0+0.0j)   # unity gain at start, 0 dB in = 0 dB out
   * (1+0j)
   * >>> agc.gain_db           # loop already advanced from 0 dB
   * 0.0
   * >>> agc2 = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
   * >>> agc2.step(4.0+0.0j)  # 12 dB loud; first sample passes at unity gain
   * (4+0j)
   * >>> round(agc2.gain_db, 6)  # loop starts driving gain negative
   * -0.024276
   * @endcode
   */
  JM_FORCEINLINE JM_HOT float complex
  agc_step (agc_state_t *state, float complex x)
  {
    /* Stage 1: linear-in-dB gain, held across the update period.  At the
     * start of each period (gain_phase == 0) refresh the linear gain from
     * the loop's command — the only exp10 on the gain path.  For the
     * default gain_update_period == 1 this runs every sample, so g_last is
     * always 10^(gain_db/20) and the path is the exact per-sample loop; for
     * P > 1 the gain is a zero-order hold and the exp10 is amortised over P.
     * g_last (= the gain actually applied this period) also seeds a
     * following agc_steps() ramp and backs applied_gain_db. */
    size_t period = state->gain_update_period ? state->gain_update_period : 1;
    if (state->gain_phase == 0)
      state->g_last = agc_exp10_ (state->gain_db * 0.05);
    float complex y = x * (float)state->g_last;

    /* Stage 2: power detector — runs every sample (cheap, no transcendental).
     * Instantaneous output power folded into the EMA p_avg += alpha*(p-p_avg)
     * exactly as the per-sample loop, so the detector trajectory is unchanged
     * by the period; only the loop-filter command below is decimated. */
    double p = agc_power_ (y);
    state->p_avg += state->alpha * (p - state->p_avg);

    /* Stage 3: 1st-order loop filter — once per period.  Integrate the dB
     * error with step size period*4*loop_bw, so the integrator advances at
     * the same per-sample-equivalent rate it would running every sample (the
     * exp10/log10 — the floor keeps log10 finite during silence — and the
     * clip-level exp10 are amortised across the period). */
    if (++state->gain_phase >= period)
      {
        double meas_db = 10.0 * agc_log10_ (state->p_avg + AGC_POWER_FLOOR);
        state->gain_db
            += (double)period * 4.0 * state->loop_bw
               * (state->ref_db - meas_db);
        state->clip_lin = (float)agc_exp10_ (state->clip_db * 0.05);
        state->gain_phase = 0;
      }

    /* Output clip — square clip (I and Q independent) to the cached level,
     * via the shared util primitive.  Applied to the returned sample only;
     * the detector above used the unclipped y, so the loop is unaffected. */
    return square_clip (y, state->clip_lin);
  }

  /**
   * @brief Process a block of complex samples through the decimated AGC loop.
   * Splits the input into chunks of @c decim samples.  Within each chunk
   * the gain is linearly interpolated from the previous chunk's end value
   * to the new loop-filter output (a first-order hold) so there is no
   * inter-chunk gain staircase.  The detector and loop filter run once per
   * chunk on the chunk's mean power — O(n/decim) control-loop work versus
   * O(n) for agc_step().  The output array may alias the input (in-place).
   * @param state   Must be non-NULL.
   * @param input   Input complex64 array of @p n samples.
   * @param output  Output buffer; must hold at least @p n elements.
   *                May alias @p input for in-place operation.
   * @param n       Number of samples to process.
   * @code
   * >>> from doppler.agc import AGC
   * >>> import numpy as np
   * >>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
   * >>> _ = agc.steps(np.full(1000, 4.0+0.0j, dtype=np.complex64))
   * >>> round(agc.gain_db, 1)   # gain converged to -12 dB
   * -12.0
   * >>> x = np.full(8, 4.0+0.0j, dtype=np.complex64)
   * >>> y = agc.steps(x)
   * >>> y.shape, y.dtype
   * ((8,), dtype('complex64'))
   * >>> [round(abs(v)**2, 2) for v in y.tolist()]  # output power ~1.0
   * [1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0]
   * @endcode
   */
  void agc_steps (agc_state_t *state, const float complex *input,
                  float complex *output, size_t n);

  /**
   * @brief Return the gain (in dB) actually applied to the most recent sample.
   * Computes @c 20*log10(g_last), where @c g_last is the linear multiplier
   * that was used on the most recently processed sample.  This differs from
   * @c gain_db (the loop integrator's current command) because the loop
   * filter advances the command one step ahead after each sample: immediately
   * after agc_step() @c gain_db already reflects the updated command while
   * @c applied_gain_db still reflects what the signal actually saw.  At
   * loop convergence the two values are numerically equal.  At create/reset
   * both are 0.0 dB (unity).
   * @return  Applied gain in dB; 0.0 at create / reset.
   * @code
   * >>> from doppler.agc import AGC
   * >>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
   * >>> agc.applied_gain_db   # unity before any sample
   * 0.0
   * >>> _ = agc.step(4.0+0.0j)
   * >>> agc.applied_gain_db   # gain USED on that sample was still 0 dB
   * 0.0
   * >>> round(agc.gain_db, 6)  # loop already advanced to new command
   * -0.024276
   * @endcode
   */
double agc_get_applied_gain_db(const agc_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
   * Whole-struct POD snapshot (pointer-free); the loop integrator, detector EMA, and ramp memory resume exactly into an
   * identically-built instance. */
#define AGC_STATE_MAGIC DP_FOURCC ('A', 'G', 'C', ' ')
#define AGC_STATE_VERSION 2u /* v2: gain_update_period + gain_phase/clip_lin */
  size_t agc_state_bytes (const agc_state_t *state);
  void    agc_get_state (const agc_state_t *state, void *blob);
  int     agc_set_state (agc_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* AGC_CORE_H */
