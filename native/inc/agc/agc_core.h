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
 * roughly @c 1/(4*loop_bw) samples, independent of the absolute signal
 * level.
 *
 * @par ...but that is the FILTER, and the object is not the filter
 * The reduction above treats @c px_db as given.  It is not: the detector is
 * inside this loop and it measures in POWER, so a quiet input's dB reading
 * approaches from the wrong side of a concave log and crawls.  The
 * level-independence therefore belongs to the loop filter alone, and the
 * OBJECT's settling is level-dependent.  Measured, 1/e settling at
 * @c loop_bw 0.005 against a predicted 50 samples:
 *
 *   +40 dB in: 41    +20 dB: 45    -20 dB: 84    -40 dB: 109
 *
 * The asymmetry scales with the detector's own bandwidth — the spread
 * between a +40 dB and a -40 dB input is 0.21 at @c alpha 0.2, 1.01 at
 * 0.05, and 2.98 at 0.01.  A composing receiver sizing a warm-up budget
 * from @c 1/(4*loop_bw) alone will be optimistic by up to 3x on a weak
 * signal.  See docs/design/agc.md section 6.
 *
 * @par Power detector
 * @c p_avg is an exponential moving average (1-pole leaky integrator) of
 * the instantaneous output power @c |y|^2.  @c alpha in (0, 1] sets the
 * detector bandwidth: small @c alpha smooths hard but reacts slowly.
 *
 * Its input is this object's ONE safety boundary — see @ref AGC_POWER_CEIL.
 * The EMA is where an input sample first becomes persistent state, so it is
 * the only place a malformed input can do lasting damage, and the only
 * place guarded.
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
 * loop, only equivalent at convergence.  Both per-block coefficients are
 * COMPOUNDED from @c alpha / @c loop_bw internally — @c 1-(1-a)^decim, not
 * @c decim*a — so both keep their per-sample meaning exactly, including at
 * @c decim==1.
 *
 * @par Choosing decim — one number
 * @parblock
 * @c 4*decim*loop_bw is how far the loop moves within one chunk, and it is
 * the only quantity that decides whether @c decim is free:
 *
 * | @c 4*decim*loop_bw | gain falling | gain rising | worst |
 * | ------------------ | ------------ | ----------- | ------- |
 * | 0.008              | 0.015 dB     | 0.054 dB    | 0.05 dB |
 * | 0.032              | 0.059 dB     | 0.197 dB    | 0.20 dB |
 * | 0.128              | 0.281 dB     | 0.592 dB    | 0.59 dB |
 * | 0.320              | 1.08 dB      | 0.91 dB     | 1.08 dB |
 * | 0.640              | 3.73 dB      | 0.97 dB     | 3.73 dB |
 *
 * **Keep @c 4*decim*loop_bw at or below 0.05 and @c decim costs under
 * 0.3 dB of transient** — pick it for throughput and forget it.  Below
 * that the worst case scales roughly as 6x the number, so halving the
 * group halves the error; above it @c decim becomes a tuning parameter
 * you have to account for.
 *
 * Both directions are quoted because this loop is not symmetric: the
 * detector is inside it and measures POWER, so a rising gain (weak input)
 * costs about 4x a falling one at the same group, and is the direction the
 * rule is set by.  It is the same asymmetry the @c loop_bw parameter note
 * describes for settling time.
 *
 * The steady state is unaffected either way; this is about the shape of
 * the acquisition.  What sets the bound is the first-order hold, not the
 * coefficients: a longer chunk ramps the gain over a longer span, so the
 * detector sees a different signal.  That is also why it cannot be
 * compounded away.
 * @endparblock
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
#include "dp_tlm/dp_tlm_core.h"
#include "util/util_core.h"
#include <math.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Power floor for the detector, in linear units.
 *
 * The low end of @ref agc_log10_'s saturation range, so a long run of
 * silence yields a large-but-finite measured level — exactly -300 dB —
 * instead of @c -INF / @c NaN, and the @c log10() argument stays a normal
 * (non-denormal) double.  The floor lives inside the primitive rather than
 * at each call site, so that promise is structural and not something a
 * caller has to remember to add.
 *
 * @par It IS reached, and used to be fatal
 * This said "never reached in normal operation — @c p_avg is seeded with
 * the reference power at create/reset", which is true of the seed and says
 * nothing about the steady state.  Any gap in the signal reaches it: a
 * muted source, a stream discontinuity, a receiver started on a zero-filled
 * buffer.  Measured on the unguarded object, ~800 silent samples — 100
 * symbols at 8 samples per symbol — left the loop permanently dead, because
 * reaching the floor gave the filter a constant +300 dB error to integrate.
 * @ref AGC_POWER_CEIL is the guard that makes reaching it survivable.
 */
#define AGC_POWER_FLOOR 1e-30

/**
 * @brief Power ceiling for the detector, in linear units.
 *
 * The largest @c |y|^2 a pair of finite @c float components can produce:
 * @c 2*FLT_MAX^2.  Any measured power above this came from a non-finite
 * output, which in turn came from a non-finite input or an overflowed gain
 * — never from a signal.
 *
 * @par The detector's input is the AGC's one safety boundary
 * Every power reaching the EMA is put through @ref saturate into
 * `[0, AGC_POWER_CEIL]`, with NaN sent to the **ceiling** — an unknown
 * level must drive the gain DOWN, since too little gain loses a signal
 * while too much rails everything downstream.
 *
 * That boundary, and not the stages around it, because the EMA is the
 * first place an input sample becomes *persistent* state.  The gain
 * multiply ahead of it is transient: a bad sample makes one bad output
 * sample and is gone.  Once it folds into @c p_avg it is remembered, and
 * the measured level, the loop integrator and the applied gain are all
 * functions of @c p_avg.  One guard here makes the whole chain total,
 * where a clamp at each stage would be several chances to miss one.
 *
 * It is sufficient because a guarded @c p_avg is a convex combination of a
 * finite @c p_avg and a saturated @c p, so it cannot leave the interval
 * once it starts inside — which @c agc_create() and @c agc_reset()
 * guarantee by seeding it with the reference power.  Measured on the
 * unguarded loop, a *single* non-finite input sample drove @c p_avg to NaN
 * permanently, and a following normal sample did not recover it.
 */
#define AGC_POWER_CEIL 2.3158417847463238e77

/**
 * @brief Default envelope decimation factor (agc_state_t::decim).
 *
 * agc_steps() runs the detector + loop filter once per chunk of
 * @c decim samples; useful values are 8, 16 and 32.  The rule is
 * @c 4*decim*loop_bw <= 0.05 (see "Choosing decim" above), which 8
 * satisfies at every loop bandwidth this object is used at; it is also one
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
   *
   * @par Total, because the exponent assembly is not
   * @c z is saturated into the range the exponent field can hold before it
   * is used.  Without that, assembling @c ((int64_t)zi + 1023) << 52
   * overflows into the SIGN bit for @c |v| past ~308, and the function
   * returns a **negative** result where the true answer is @c +inf or 0 —
   * measured, @c agc_exp10_(309) gave @c -3.09e-308 and
   * @c agc_exp10_(-320) gave @c -3.23e+296.  A gain function that returns a
   * negative gain does not merely lose precision, it inverts the signal.
   * Past the rails this now saturates at @c 2^±1023 instead.
   *
   * NaN takes the LOW rail, and the direction is the same one
   * @ref AGC_POWER_CEIL uses: when the input is unknown, attenuate.  A gain
   * saturated low is silence; a gain saturated high rails everything
   * downstream of it.
   */
  JM_FORCEINLINE double
  agc_exp10_ (double v)
  {
    /* Bound BEFORE floor(): (int64_t) of a huge double is itself undefined,
       so the saturation cannot wait until the cast. */
    double z = saturate (v * 3.321928094887362, /* z = v * log2(10) */
                         -1023.0, 1023.0, -1023.0);
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
   *
   * @par Total, and it must be — it reads the exponent field directly
   * @p p is saturated into `[AGC_POWER_FLOOR, AGC_POWER_CEIL]` first.  The
   * bit-field split has no notion of a special value: handed a NaN it reads
   * the exponent as an ordinary 1024 and returns a perfectly plausible
   * @c +308, where libm's @c log10 returns NaN.  Measured on the unguarded
   * version, that fabricated level was what turned a stalled AGC into a
   * runaway one — the loop believed it was seeing @c +3084 dB and drove the
   * gain the other way, forever.  A wrong answer that looks like a right
   * one is worse than an infinity.
   *
   * The floor is why silence reads as about @c -300 dB rather than
   * @c -INF, so that promise is now structural rather than something each
   * caller has to remember to add.
   */
  JM_FORCEINLINE double
  agc_log10_ (double p)
  {
    /* NaN to the CEILING: for a measured LEVEL, unknown must read loud, so
       the loop it feeds turns the gain down.  Same rule as AGC_POWER_CEIL,
       stated the other way round from agc_exp10_'s because this is a level
       and that is a gain. */
    p = saturate (p, AGC_POWER_FLOOR, AGC_POWER_CEIL, AGC_POWER_CEIL);
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
   * @brief Telemetry attachment: a borrowed context + this object's probe
   *        ids.  NULL ctx (the default) means detached — every probe site
   *        is then a single predicted-not-taken branch.  Zeroed in state
   *        blobs and preserved across set_state (DP_DEFINE_POD_STATE_TLM);
   *        telemetry is observation, not DSP state that migrates.
   */
  typedef struct
  {
    dp_tlm_t *ctx;      /* NULL = detached                    */
    int32_t   id_gain;  /* probe ids from a successful attach  */
    int32_t   id_level; /* (fills what used to be _pad)        */
  } agc_tlm_t;

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
     * latency for speed (keep well below 1/(4*loop_bw)).  The same shape
     * as decim's `4*decim*loop_bw <= 0.05`, but only decim's is measured —
     * this one is a zero-order hold on a different quantity. */
    size_t gain_update_period;
    double gain_db; /* loop-filter integrator: current gain, dB        */
    /* Power-detector EMA of OUTPUT power, linear. Anything setting this by
       hand must use the REFERENCE power, not a measured input power: the loop
       filter's error is (ref_db - 10log10(p_avg)), so seeding it with an
       input measurement hands the loop an error equal to the whole gain and
       it integrates it. Measured on this object at loop_bw 0.002 / alpha
       0.01, a 4x-hot input drove the gain a further 13.4 dB past its correct
       value before recovering (+2.4 dB at 0.25x). agc_create()/agc_reset()
       already do the right thing; this note is for anyone tempted to
       shortcut them. */
    double p_avg;
    double g_last;  /* current linear gain held across the period      */
    size_t gain_phase; /* agc_step() position in the update period     */
    float  clip_lin;   /* cached 10^(clip_db/20), refreshed per period */
    agc_tlm_t tlm; /* live telemetry attachment; zeroed in blobs        */
  } agc_state_t;

  /**
   * @brief Construct a log-domain feedback AGC and return its heap state.
   * The loop integrator starts at 0 dB (unity gain) and the power detector
   * @c p_avg is pre-seeded to @c 10^(ref_db/10) linear, so the first block
   * of on-target samples produces no transient.  Three parameters tune the
   * closed-loop behaviour: @p ref_db sets the target, @p loop_bw sets the
   * convergence speed, and @p alpha sets the detector smoothing.
   * @param ref_db   Target output power in dB (e.g. @c 0.0 for unity power).
   * @param loop_bw  Loop noise bandwidth in cycles/sample.  The FILTER's
   *                 time constant is @c 1/(4*loop_bw) samples; the object
   *                 settles more slowly than that on a quiet input, because
   *                 the detector is inside the loop and measures in power
   *                 (see the Linear-in-dB note above — measured 1.7x to
   *                 2.2x at -40 dB in, worse at small @p alpha).  Treat
   *                 @c 1/(4*loop_bw) as a floor on settling, not an
   *                 estimate of it.  Smaller values are slower and
   *                 smoother.  With agc_steps(), the pairing rule is
   *                 @c 4*decim*loop_bw <= 0.05 — see "Choosing decim".
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
   * >>> agc2.step(4.0+0.0j)  # 12 dB loud; first sample at unity gain
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
    double p     = agc_power_ (y);
    state->p_avg = ema_step (
        state->p_avg, saturate (p, 0.0, AGC_POWER_CEIL, AGC_POWER_CEIL),
        state->alpha);

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
        /* Telemetry tap — per gain-update event (already amortised by the
         * period), one branch when detached.  `meas_db` is the level the
         * detector believes it has BEFORE this update's correction, so the
         * pair reads as (command, what it was answering). */
        DP_TLM (state->tlm.ctx, state->tlm.id_gain, state->gain_db);
        DP_TLM (state->tlm.ctx, state->tlm.id_level, meas_db);
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

  /**
   * @brief How many samples this loop needs to settle — the design query.
   *
   * Answers "how long must I wait before the output level can be trusted",
   * which a caller sizing a warm-up budget, a burst preamble or an
   * acquisition guard has to answer and could not.
   *
   * @par Why the header's time constant is not the answer
   * @c 1/(4*loop_bw) is the loop FILTER's time constant, and the object is
   * not the filter — the detector sits inside the loop and measures in
   * power, so a quiet input settles more slowly (see the Linear-in-dB note
   * above).  The real settling is @c M/(4*loop_bw) where @c M depends on
   * the starting error and on how fast the detector is relative to the
   * filter, @c alpha/(4*loop_bw).  Measured, @c M runs from about 0.8 on a
   * loud start to nearly 5 on a quiet one with a slow detector.
   *
   * @par It measures rather than approximates
   * This runs the real @ref agc_step loop against a constant input and
   * counts, so there is no fitted curve to go stale: the answer is
   * whatever the shipped loop does, and it cannot disagree with the object
   * it describes.  Design-time only — it allocates and iterates, so call
   * it while planning a pipeline, never inside one.
   *
   * @param loop_bw      Loop noise bandwidth, as passed to agc_create().
   * @param alpha        Detector EMA coefficient, as passed to
   *                     agc_create().
   * @param gain_err_db  How far from settled the loop starts, in dB of
   *                     gain it must apply.  POSITIVE for a quiet input
   *                     (the loop must add gain) — the slow direction, and
   *                     the one to budget for.  For a cold receiver this
   *                     is the whole input dynamic range it must cover,
   *                     not the steady-state variation.
   * @param tol_db       Settled means within this many dB of the target.
   * @return Samples to settle (>= 1), or 0 if the arguments are invalid or
   *         the loop does not settle within a bounded search.
   * @code
   * >>> from doppler.agc import settling_samples
   * >>> settling_samples(0.0025, 0.05, 40.0, 0.5)   # cold, 40 dB quiet
   * 430
   * >>> settling_samples(0.0025, 0.05, 40.0, 3.0)   # a looser bar is cheaper
   * 294
   * >>> settling_samples(0.0025, 0.05, -40.0, 0.5)  # loud: the fast direction
   * 175
   * >>> settling_samples(0.01, 0.05, 40.0, 0.5)     # 4x the bandwidth, ~1/4
   * 112
   * >>> settling_samples(0.0025, 0.05, 0.1, 0.5)    # already inside tol_db
   * 1
   * >>> settling_samples(0.0, 0.05, 40.0, 0.5)      # refused, not guessed
   * 0
   * @endcode
   */
  size_t agc_settling_samples (double loop_bw, double alpha,
                               double gain_err_db, double tol_db);

  /**
   * @brief Attach (or detach) a telemetry context and register the AGC's
   * probes on it.
   * Registers two probes, both recorded once per gain-update event and
   * further thinned by decim:
   *
   *   - "<prefix>.gain_db" — the loop-filter integrator, i.e. the gain the
   *     loop is commanding, in dB.
   *   - "<prefix>.level_db" — the level the power detector measures,
   *     `10*log10(p_avg)`, in dB.  This is the loop's *input*: the
   *     integrator drives `ref_db - level_db` to zero, so level_db is the
   *     zero-referenced settling indicator.  Reading it says whether the
   *     loop has converged without knowing the true input level, which
   *     gain_db alone cannot — gain_db settles to an offset that depends
   *     on how loud the signal happens to be.
   *
   * The pair is emitted from one update, with level_db being the belief
   * that update was answering (measured before the correction is applied).
   * Passing NULL detaches (probe sites revert
   * to their single-branch disabled cost); re-attaching after a reset is
   * idempotent (same name -> same probe id).  Setup path, never hot: call
   * before the producer thread starts stepping, and keep every object
   * attached to one context on that one thread (the ring is SPSC — see
   * dp_tlm/dp_tlm_core.h).  The context is borrowed, not owned: it must
   * outlive the attachment.
   * @param state  Must be non-NULL.
   * @param tlm    Telemetry context to attach, or NULL to detach.
   * @param prefix Probe-name prefix, e.g. "agc" or "rx.agc".
   * @param decim  Emit every decim-th gain update; >= 1.
   * @return DP_OK, or DP_ERR_INVALID when the probe table cannot take both
   *         probes or a prefixed name is invalid (the attach fails whole;
   *         the object stays detached).
   * @code
   * >>> import numpy as np
   * >>> from doppler.agc import AGC
   * >>> from doppler.telemetry import Telemetry
   * >>> tlm = Telemetry(1 << 12)
   * >>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
   * >>> agc.set_telemetry(tlm, "agc")
   * >>> sorted(tlm.probe_names)
   * ['agc.gain_db', 'agc.level_db']
   * >>> x = (0.5 + 0j) * np.ones(4096, dtype=np.complex64)
   * >>> _ = agc.steps(x)
   * >>> recs = tlm.read()      # both probes, per decim-chunk update
   * >>> gain = recs[recs["probe"] == tlm.probe_id("agc.gain_db")]["value"]
   * >>> lvl = recs[recs["probe"] == tlm.probe_id("agc.level_db")]["value"]
   * >>> len(gain) == len(lvl) == 4096 // agc.decim
   * True
   * >>> round(float(gain[-1]), 1)   # -6 dB input, 0 dB ref -> +6 dB gain
   * 6.0
   * >>> round(float(lvl[-1]), 1)    # settled: measured level == ref
   * 0.0
   *
   * @endcode
   */
int agc_set_telemetry(agc_state_t *state, dp_tlm_t * tlm, const char * prefix, uint32_t decim);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
   * Whole-struct POD snapshot (pointer-free); the loop integrator, detector EMA, and ramp memory resume exactly into an
   * identically-built instance.  The telemetry attachment is zeroed in
   * blobs and preserved across restore (DP_DEFINE_POD_STATE_TLM). */
#define AGC_STATE_MAGIC DP_FOURCC ('A', 'G', 'C', ' ')
#define AGC_STATE_VERSION 3u /* v3: telemetry attachment (zeroed in blob) */
  size_t agc_state_bytes (const agc_state_t *state);
  void    agc_get_state (const agc_state_t *state, void *blob);
  int     agc_set_state (agc_state_t *state, const void *blob);

size_t settling_samples(double loop_bw, double alpha, double gain_err_db, double tol_db);
#ifdef __cplusplus
}
#endif

#endif /* AGC_CORE_H */
