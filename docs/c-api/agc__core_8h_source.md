

# File agc\_core.h

[**File List**](files.md) **>** [**agc**](dir_947ec4d62e9dda8dbffe026d57cfb18d.md) **>** [**agc\_core.h**](agc__core_8h.md)

[Go to the documentation of this file](agc__core_8h.md)


```C++

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

#define AGC_POWER_FLOOR 1e-30

#define AGC_DECIM_DEFAULT 8

#define AGC_CLIP_DB_DEFAULT 120.0

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

  JM_FORCEINLINE double
  agc_power_ (float complex y)
  {
    double yr = (double)crealf (y), yi = (double)cimagf (y);
    return yr * yr + yi * yi;
  }

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

agc_state_t *agc_create(double ref_db, double loop_bw, double alpha);

void agc_destroy(agc_state_t *state);

void agc_reset(agc_state_t *state);

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

  void agc_steps (agc_state_t *state, const float complex *input,
                  float complex *output, size_t n);

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
```


