

# File ratesync\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**ratesync**](dir_bd24358a1650cccc3777ef85b64503d5.md) **>** [**ratesync\_core.h**](ratesync__core_8h.md)

[Go to the documentation of this file](ratesync__core_8h.md)


```C++

#ifndef RATESYNC_CORE_H
#define RATESYNC_CORE_H

#include "RateConverter/RateConverter_core.h"
#include "cic/cic_core.h"
#include "clib_common.h"
#include "dp_state.h"
#include "fir/fir_core.h"
#include "hbdecim/hbdecim_core.h"
#include "jm_perf.h"
#include "lockdet/lockdet_core.h"
#include "loop_filter/loop_filter_core.h"
#include "resamp/resamp_core.h"
#include "resample/resample_core.h"
#include "symsync/symsync_core.h" /* gardner_ted / dttl_ted — one TED, reused */
#include "dp_tlm/dp_tlm_core.h"
#include "telemetry/telemetry_core.h"
#include "ber/ber_core.h"
#include "pn/pn_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  enum
  {
    RATESYNC_TED_GARDNER = 0, 
    RATESYNC_TED_DTTL    = 1  
  };

  enum
  {
    RATESYNC_PULSE_IANDD = RC_PULSE_IANDD, 
    RATESYNC_PULSE_RRC   = RC_PULSE_RRC    
  };

#define RATESYNC_MAX_M 8

/* Numerical guard on the on-time+mid energy sum feeding the lock statistic
 * (not tunable) — mirrors SYMSYNC_LOCK_EPS. */
#define RATESYNC_LOCK_EPS 1e-12

  typedef struct
  {
    dp_tlm_t *ctx;       
    int32_t   id_e;      
    int32_t   id_ctrl;   
    int32_t   id_rate;   
    int32_t   id_lock;   
    int32_t   id_locked; 
    int32_t   id_mu;     
  } ratesync_tlm_t;

  typedef struct
  {
    loop_filter_state_t lf; 
    /* ── config (restored by the owner's create(), never packed) ────── */
    double sps;        
    size_t m;          
    double term_rate;  
    size_t prime_taps; 
    const resamp_state_t *term;
    double bn;         
    double zeta;       
    int    ted;        
    double ted_scale;

    /* ── running state ──────────────────────────────────────────────── */
    double ctrl;       
    double last_error; 
    double rate_est;   
    int    have_prev;  
    size_t prime_left; 
    size_t out_count;  
    float complex ring[RATESYNC_MAX_M / 2 + 1];
    size_t        ring_n;
    float complex prev_on; 
    /* ── lock detector (always on): tumbling-window block average ───── */
    double lock_sum;      
    size_t lock_count;    
    size_t avgs;          
    double lock_stat;     
    lockdet_state_t lock; 
    ratesync_tlm_t tlm; 
  } ratesync_loop_t;

  typedef struct
  {
    RateConverter_state_t *mf;   
    ratesync_loop_t        loop; 
    /* ── config (restored by create(), never packed in a state blob) ── */
    int    pulse;      
    double beta;       
    size_t span;       
    size_t num_phases; 
  } ratesync_state_t;

  /* ------------------------------------------------------------------
   * The timing loop on its own (shared with the receivers)
   * ------------------------------------------------------------------ */

  void ratesync_loop_init (ratesync_loop_t *l, double sps, size_t m, double bn,
                           double zeta, int ted);

  void ratesync_loop_set_cascade (ratesync_loop_t *l, double term_rate,
                                  size_t prime_taps);

  void ratesync_loop_bind_cascade (ratesync_loop_t             *l,
                                   const RateConverter_state_t *rc);

  void ratesync_loop_reset (ratesync_loop_t *l);

  void ratesync_loop_configure (ratesync_loop_t *l, double bn, double zeta);

  void ratesync_loop_configure_lock_raw (ratesync_loop_t *l, size_t avgs,
                                         double up_thresh, double down_thresh,
                                         uint32_t n_up, uint32_t n_down);

  int ratesync_loop_set_telemetry (ratesync_loop_t *l, dp_tlm_t *tlm,
                                   const char *prefix, uint32_t decim);

  void ratesync_loop_tlm_flush (const ratesync_loop_t *l);

/* ── Serializable state — the loop alone (nested by every owner) ───────────
 * Envelope, this loop's running scalars, then the loop filter's own
 * self-validating sub-blob. Config (sps/m/term_rate/prime_taps/bn/zeta/ted)
 * is restored by the owner's create() and never packed. */
#define RATESYNC_LOOP_STATE_MAGIC DP_FOURCC ('R', 'S', 'L', 'P')
#define RATESYNC_LOOP_STATE_VERSION 2u /* v2: the TED normaliser is
                                        * a construct-time constant, so
                                        * pwr_avg/pwr_seeded are gone */

  size_t ratesync_loop_state_bytes (const ratesync_loop_t *l);
  void ratesync_loop_get_state (const ratesync_loop_t *l, void *blob);
  int ratesync_loop_set_state (ratesync_loop_t *l, const void *blob);

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  ratesync_state_t *ratesync_create (double sps, int pulse, double beta,
                                     size_t span, size_t m, size_t num_phases,
                                     double bn, double zeta, int ted);

  void ratesync_destroy (ratesync_state_t *state);

  void ratesync_reset (ratesync_state_t *state);

  /* ------------------------------------------------------------------
   * Execute
   * ------------------------------------------------------------------ */

  JM_FORCEINLINE JM_HOT int
  ratesync_loop_take_output (ratesync_loop_t *s, float complex y,
                             float complex *y_out, int ted)
  {
    /* Newest-first ring: ring[0] is this output, ring[m/2] the gate. */
    const size_t half = s->m >> 1;
    for (size_t i = s->ring_n < half ? s->ring_n : half; i > 0; i--)
      s->ring[i] = s->ring[i - 1];
    s->ring[0] = y;
    if (s->ring_n <= half)
      s->ring_n++;

    if (s->prime_left)
      {
        /* The cascade is still filling: these outputs are the delay lines,
           not the signal. Drop them without advancing the strobe phase, so
           the first real strobe starts a clean count. */
        s->prime_left--;
        return 0;
      }

    size_t phase = s->out_count++ % s->m;
    if (phase != 0 || s->ring_n <= half)
      return 0; /* not a strobe, or the gate is not yet in the ring */

    const float complex on  = y;
    const float complex mid = s->ring[half];
    if (!s->have_prev)
      {
        s->have_prev = 1;
        s->prev_on   = on;
        return 0;
      }

    double num;
    if (ted == RATESYNC_TED_DTTL)
      num = dttl_ted (mid, on, s->prev_on);
    else
      num = gardner_ted (mid, on - s->prev_on);

    double on_pwr
        = (double)(crealf (on) * crealf (on) + cimagf (on) * cimagf (on));
    double mid_pwr
        = (double)(crealf (mid) * crealf (mid) + cimagf (mid) * cimagf (mid));
    /* `ref` is the lock statistic's normaliser, and ONLY that — it is an
       instantaneous ratio, so it needs no averaging and cannot go stale. */
    double ref = on_pwr + mid_pwr;

    /* The detector's own slope, divided out by a construct-time reciprocal.
       Amplitude does not appear: it enters the raw error as A^2 (Gardner) or
       A^1 (DTTL), and a unity-gain matched cascade delivers the amplitude it
       was sent, so levelling the signal is an AGC's job upstream — not a
       running estimate inside the detector. Transition density does not
       appear either; it is data, and whatever slope it yields is the honest
       slope.
         What this replaces was a 1%-per-symbol average of |on|^2+|mid|^2.
       Two things were wrong with it. It is an A^2 quantity, so it was right
       for Gardner's amplitude law and left DTTL's gain proportional to 1/A —
       a 4x swing over a 4x level change, in the detector BPSK selects. And
       being an average, it lagged: seeded on the first post-prime strobe,
       which lands in the cascade's amplitude ramp, it ran the loop at up to
       thousands of times its designed gain for exactly the interval that
       decides acquisition. Measured, that wound the integrator past pull-in
       and cost 7000-25000 symbols to recover across a 0.3-symbol-wide band
       of initial offsets; with the lag gone the same band acquires in
       133-266. */
    double e      = num * s->ted_scale;
    s->last_error = e;

    /* loop_filter_step returns a correction in symbols per symbol; `ctrl` is
       a rate deviation the TERMINAL stage adds to its accumulator once per
       one of ITS OWN inputs — not once per cascade input. Those differ by the
       whole integer decimation in front, so scaling by the cascade rate m/sps
       under-drives the loop by exactly that factor (32x at sps=64 behind a
       CIC(32), which is why it could barely track).
         Over one symbol the terminal stage sees N = m/rate_term inputs, so
       the accumulator gains N*ctrl output periods = N*ctrl/m symbols; setting
       that equal to the requested correction gives ctrl = correction *
       rate_term, with no reference to sps or the decimation at all.
       e > 0 means the strobe is LATE and a positive ctrl advances it — the
       classic Gardner polarity. */
    s->ctrl = loop_filter_step (&s->lf, e) * s->term_rate;

    /* Tracked samples/symbol from the loop INTEGRATOR, not the instantaneous
       control. The integrator is the rate memory (loop_filter_core.h: "kp*e
       is the instantaneous phase nudge"); feeding the noisy total through the
       convex 1/(1+integ) instead biases the estimate high by Jensen. Exact
       here: a loop tracking true sps settles at integ = sps/true - 1, so
       sps/(1 + integ) == true. */
    double inst = s->sps / (1.0 + s->lf.integ);
    double lo_r = 0.5 * s->sps, hi_r = 1.5 * s->sps;
    if (!(inst > lo_r))
      inst = lo_r;
    else if (inst > hi_r)
      inst = hi_r;
    s->rate_est = inst;

    /* Lock statistic: the Gardner eye-opening ratio
       2*(|on|^2 - |mid|^2)/(|on|^2 + |mid|^2) — an open eye means the on-time
       strobe carries the energy and the transition gate sits near zero.
       Block-averaged over `avgs` looks before the decision (a tumbling
       window, so verify counts stay independent), mirroring symsync/dll. */
    double lock_signal = 2.0 * (on_pwr - mid_pwr) / (ref + RATESYNC_LOCK_EPS);
    s->lock_sum += lock_signal;
    if (++s->lock_count >= s->avgs)
      {
        s->lock_stat = s->lock_sum / (double)s->avgs;
        (void)lockdet_step (&s->lock, s->lock_stat);
        s->lock_sum   = 0.0;
        s->lock_count = 0;
      }

    s->prev_on = on;
    *y_out     = on;
    return 1;
  }

  JM_FORCEINLINE JM_HOT int
  ratesync_step_ted (ratesync_state_t *s, float complex x,
                     float complex *y_out, int ted)
  {
    /* One input can complete MORE THAN ONE output period. It happens
       whenever the terminal stage's own rate is at or near 1.0 (a cascade
       like HB + Resampler(1.0), which is what an integer sps plans) and the
       control has pushed the accumulator over: that input emits two. Asking
       for only one silently DROPS the second, which permanently shifts the
       strobe parity and leaves the loop sliding — measured as `rate_est`
       walking monotonically away while the eye never opens. The cascade rate
       is m/sps <= 1, so an input can complete at most two output periods, and
       with m >= 2 those can contain at most one on-time strobe: the
       single-symbol return of this function is still correct. */
    float complex ys[4];
    size_t n = RateConverter_execute_ctrl_push (s->mf, x, s->loop.ctrl, ys,
                                                sizeof (ys) / sizeof (ys[0]));
    int    emitted = 0;
    for (size_t oi = 0; oi < n; oi++)
      emitted |= ratesync_loop_take_output (&s->loop, ys[oi], y_out, ted);
    return emitted;
  }

  JM_FORCEINLINE JM_HOT int
  ratesync_step (ratesync_state_t *s, float complex x, float complex *y_out)
  {
    int r = ratesync_step_ted (s, x, y_out, s->loop.ted);
    if (r && s->loop.tlm.ctx)
      ratesync_loop_tlm_flush (&s->loop);
    return r;
  }

  size_t ratesync_steps_max_out (ratesync_state_t *state);

  size_t ratesync_steps (ratesync_state_t *state, const float complex *x,
                         size_t x_len, float complex *out, size_t max_out);

  /* ------------------------------------------------------------------
   * Properties / configuration
   * ------------------------------------------------------------------ */

  void   ratesync_configure (ratesync_state_t *state, double bn, double zeta);
  double ratesync_get_bn (const ratesync_state_t *state);
  void   ratesync_set_bn (ratesync_state_t *state, double val);

  double ratesync_get_timing_error (const ratesync_state_t *state);

  double ratesync_get_rate (const ratesync_state_t *state);

  double ratesync_get_ctrl (const ratesync_state_t *state);

  double ratesync_get_lock_stat (const ratesync_state_t *state);

  int ratesync_get_locked (const ratesync_state_t *state);

  int ratesync_get_clipped (const ratesync_state_t *state);

  void ratesync_configure_lock_raw (ratesync_state_t *state, size_t avgs,
                                    double up_thresh, double down_thresh,
                                    uint32_t n_up, uint32_t n_down);

  int ratesync_set_telemetry (ratesync_state_t *state, dp_tlm_t *tlm,
                              const char *prefix, uint32_t decim);

/* ── Serializable state (standard bytes interface; see dp_state.h) ─────────
 * A composition of exactly two children now that the timing loop is its own
 * struct: `[hdr][RateConverter][ratesync_loop]`, each self-validating. All of
 * this object's own running state moved into the loop's blob, so it packs no
 * scalars of its own; config (sps/pulse/beta/span/m/num_phases/bn/zeta/ted) is
 * restored by create() and never packed. */
#define RATESYNC_STATE_MAGIC DP_FOURCC ('R', 'A', 'T', 'S')
#define RATESYNC_STATE_VERSION 2u /* v2: running state moved into the loop */

  size_t ratesync_state_bytes (const ratesync_state_t *state);
  void ratesync_get_state (const ratesync_state_t *state, void *blob);
  int ratesync_set_state (ratesync_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* RATESYNC_CORE_H */
```


