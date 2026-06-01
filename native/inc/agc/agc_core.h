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
   * @brief AGC state.
   *
   * Allocate with agc_create().  @c ref_db, @c loop_bw, @c alpha,
   * @c decim and @c clip_db are configuration (readable and writable at
   * runtime); @c gain_db, @c p_avg and @c g_last are the loop's
   * internal memory.
   */
  typedef struct
  {
    double ref_db;  /* target output power, dB                        */
    double loop_bw; /* loop noise bandwidth, cycles/sample             */
    double alpha;   /* power-detector EMA coefficient, (0, 1]          */
    size_t decim;   /* agc_steps() chunk length (8 / 16 / 32)          */
    double clip_db; /* output square-clip level, dB (per component)    */
    double gain_db; /* loop-filter integrator: current gain, dB        */
    double p_avg;   /* power-detector EMA: averaged output power, lin  */
    double g_last;  /* last linear gain applied — ramp continuity      */
  } agc_state_t;

  /**
   * @brief Create an AGC instance.
   *
   * @param ref_db     Target output power in dB (e.g. 0.0).
   * @param loop_bw    Loop noise bandwidth, normalised (cycles/sample);
   *                   ~1/(4*loop_bw) samples to settle.  Smaller is slower
   *                   and smoother.
   * @param alpha      Power-detector EMA coefficient in (0, 1].
   * @return Heap-allocated state, or NULL on allocation failure.
   * @note @c gain_db starts at 0 dB (unity gain); @c p_avg is seeded with
   *       the reference power 10^(ref_db/10) so the loop starts settled.
   * @note Caller must call agc_destroy() when done.
   */
agc_state_t *agc_create(double ref_db, double loop_bw, double alpha);

  /**
   * @brief Destroy an AGC instance and release all memory.
   * @param state  May be NULL.
   */
void agc_destroy(agc_state_t *state);

  /**
   * @brief Reset the AGC to its post-create state.
   *
   * Zeroes @c gain_db and re-seeds @c p_avg from the current @c ref_db.
   * Configuration (@c ref_db, @c loop_bw, @c alpha, @c decim,
   * @c clip_db) is unchanged.
   * @param state  Must be non-NULL.
   */
void agc_reset(agc_state_t *state);

  /**
   * @brief Process one input sample (exact per-sample control loop).
   *
   * Applies the current gain, updates the power detector from the gained
   * output, advances the loop filter by one step, then square-clips the
   * returned sample to @c clip_db.  This is the exact reference path;
   * agc_steps() is the faster decimated equivalent.
   *
   * @param state  Must be non-NULL; mutated (gain_db, p_avg).
   * @param x      Input sample (float complex).
   * @return Output sample = x * 10^(gain_db/20), square-clipped to
   *         @c clip_db.  The clip does not feed the detector.
   */
  JM_FORCEINLINE JM_HOT float complex
  agc_step (agc_state_t *state, float complex x)
  {
    /* Stage 1: linear-in-dB gain.  gain_db is voltage dB, so the linear
     * multiplier is 10^(gain_db/20); 0.05 == 1/20.  Record it so a
     * following agc_steps() call ramps continuously from here. */
    double g = agc_exp10_ (state->gain_db * 0.05);
    state->g_last = g;
    float complex y = x * (float)g;

    /* Stage 2: power detector.  Instantaneous output power folded into
     * the EMA p_avg += alpha * (p - p_avg). */
    double yr = (double)crealf (y);
    double yi = (double)cimagf (y);
    double p = yr * yr + yi * yi;
    state->p_avg += state->alpha * (p - state->p_avg);

    /* Stage 3: 1st-order loop filter.  Integrate the dB error with step
     * size 4*loop_bw (loop_bw is the loop noise bandwidth); the floor
     * keeps log10 finite if p_avg has decayed to ~0 during silence. */
    double meas_db = 10.0 * agc_log10_ (state->p_avg + AGC_POWER_FLOOR);
    state->gain_db += 4.0 * state->loop_bw * (state->ref_db - meas_db);

    /* Output clip — square clip (I and Q independent) to the
     * programmable level, via the shared util primitive.  Applied to
     * the returned sample only; the detector above used the unclipped
     * y, so the loop is unaffected. */
    return square_clip (y, (float)agc_exp10_ (state->clip_db * 0.05));
  }

  /**
   * @brief Process a block of samples (decimated control loop).
   *
   * Processes @p input in chunks of @c state->decim samples.  The gain
   * the loop commands is linearly interpolated across each chunk (a
   * first-order hold — no inter-chunk staircase), and the detector and
   * loop filter run once per chunk on the chunk's mean power.  Not
   * bit-identical to a per-sample agc_step() loop — see the file header
   * — but equivalent at convergence.  Each output sample is then
   * square-clipped to @c clip_db; the clip is applied after the power
   * sum, so it does not feed the detector.
   *
   * @param state   Component state (mutated).
   * @param input   Input array (length >= n).
   * @param output  Output array (length >= n; may alias input in-place).
   * @param n       Number of samples.
   */
  void agc_steps (agc_state_t *state, const float complex *input,
                  float complex *output, size_t n);

  /**
   * @brief Gain actually applied to the most recent sample, in dB.
   *
   * Returns @c 20*log10 of the internal linear gain @c g_last — the gain
   * the signal last *saw*.  This is distinct from @c gain_db, which is
   * the gain the loop currently *commands*:
   *
   *  - after agc_step() the two differ, because the loop filter has
   *    already advanced past the gain it just applied;
   *  - within an agc_steps() block the applied gain is a first-order-hold
   *    ramp, and this returns the ramp's endpoint (the last sample's gain).
   *
   * Use it for telemetry when the AGC is a stage in a larger chain and a
   * downstream consumer needs to know what gain was used, not what the
   * loop last asked for.
   *
   * @param state  Must be non-NULL.
   * @return Applied gain in dB; 0.0 dB (unity) at create / reset.
   */
double agc_get_applied_gain_db(const agc_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* AGC_CORE_H */
