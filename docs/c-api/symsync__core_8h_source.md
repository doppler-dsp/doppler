

# File symsync\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**symsync**](dir_bee143323fe2e99a30a6d3a881f82f29.md) **>** [**symsync\_core.h**](symsync__core_8h.md)

[Go to the documentation of this file](symsync__core_8h.md)


```C++

#ifndef SYMSYNC_CORE_H
#define SYMSYNC_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "farrow/farrow_core.h"
#include "jm_perf.h"
#include "lockdet/lockdet_core.h"
#include "loop_filter/loop_filter_core.h"
#include "nco/nco_core.h"
#include "telemetry/telemetry.h"
#ifdef __cplusplus
extern "C"
{
#endif

  enum
  {
    SYMSYNC_TED_GARDNER = 0, 
    SYMSYNC_TED_DTTL    = 1  
  };

/* Numerical guard on the on-time+mid-symbol energy sum feeding the lock
 * statistic (not tunable). */
#define SYMSYNC_LOCK_EPS 1e-12

  typedef struct
  {
    dp_tlm_t *ctx;       
    int32_t   id_e;      
    int32_t   id_freq;   
    int32_t   id_rate;   
    int32_t   id_lock;   
    int32_t   id_locked; 
  } symsync_tlm_t;

  typedef struct
  {
    nco_state_t         timing; 
    farrow_state_t      farrow; 
    loop_filter_state_t lf;     
    size_t              sps;    
    uint32_t      base_inc;     
    int           ted;          
    int           have_ontime;  
    float complex prev_ontime;  
    float complex mid;          
    double        bn;           
    double        zeta;         
    double        last_error;   
    double        rate_est;     
    double        pwr_avg;      
    /* ── lock detector (always on): tumbling-window block average ────── */
    double lock_sum;      
    size_t lock_count;    
    size_t avgs;          
    double lock_stat;     
    lockdet_state_t lock; 
    symsync_tlm_t tlm; 
  } symsync_state_t;

  JM_FORCEINLINE double
  gardner_ted (float complex mid, float complex diff)
  {
    return (double)(crealf (mid) * crealf (diff)
                    + cimagf (mid) * cimagf (diff));
  }

  JM_FORCEINLINE double
  dttl_ted (float complex mid, float complex y, float complex prev)
  {
    double si = (crealf (y) >= 0.0f ? 1.0 : -1.0)
                - (crealf (prev) >= 0.0f ? 1.0 : -1.0);
    double sq = (cimagf (y) >= 0.0f ? 1.0 : -1.0)
                - (cimagf (prev) >= 0.0f ? 1.0 : -1.0);
    return (double)crealf (mid) * si + (double)cimagf (mid) * sq;
  }

  JM_FORCEINLINE JM_HOT int
  symsync_step_ted (symsync_state_t *s, float complex x, float complex *y_out,
                    int ted)
  {
    const uint32_t HALF = 0x80000000u;
    farrow_push (&s->farrow, x);
    uint32_t old    = s->timing.phase;
    uint64_t sum    = (uint64_t)old + s->timing.phase_inc;
    s->timing.phase = (uint32_t)sum;

    int wrapped = sum >> 32 != 0;
    int mid_evt = !wrapped && old < HALF && (uint32_t)sum >= HALF;
    if (!wrapped && !mid_evt)
      return 0;

    double inc = (double)s->timing.phase_inc;
    if (mid_evt)
      {
        float mu = (float)(1.0 - ((double)((uint32_t)sum - HALF)) / inc);
        s->mid   = farrow_eval (&s->farrow, mu);
        return 0;
      }
    float         mu   = (float)(1.0 - (double)s->timing.phase / inc);
    float complex y    = farrow_eval (&s->farrow, mu);
    int           emit = 0;
    if (s->have_ontime)
      {
        double num;
        if (ted == SYMSYNC_TED_DTTL)
          num = dttl_ted (s->mid, y, s->prev_ontime);
        else
          {
            float complex diff = y - s->prev_ontime;
            num                = gardner_ted (s->mid, diff);
          }
        double inst_pwr
            = (double)(crealf (y) * crealf (y) + cimagf (y) * cimagf (y));
        s->pwr_avg += 0.01 * (inst_pwr - s->pwr_avg);
        double e       = num / (s->pwr_avg + 1e-6);
        s->last_error  = e;
        double control = loop_filter_step (&s->lf, e);
        s->timing.phase_inc
            = (uint32_t)((double)s->base_inc * (1.0 + control));
        double inst = (double)s->sps / (1.0 + control);
        double lo_r = 0.5 * (double)s->sps, hi_r = 1.5 * (double)s->sps;
        if (inst < lo_r)
          inst = lo_r;
        else if (inst > hi_r)
          inst = hi_r;
        s->rate_est += 0.02 * (inst - s->rate_est);
        /* Lock statistic: lock_signal = 2*(|on-time|^2-|mid|^2)
         * /(|on-time|^2+|mid|^2), a Gardner-style eye-opening ratio (the
         * on-time sample vs. the mid-symbol/transition-gate sample already
         * used by the TED, reusing inst_pwr from above). Non-coherently
         * block-averaged over `avgs` looks before the decision, mirroring
         * dll_state_t's lock_sum/lock_count/n_looks tumbling window (a
         * sliding window would break the verify-count independence
         * assumption the same way it would for the DLL -- see
         * dll_configure_lock's derivation). See symsync_configure_lock()
         * for how avgs/threshold are sized from (rolloff, esno_min, pfa,
         * pd). */
        float complex md = s->mid;
        double        mid_pwr
            = (double)(crealf (md) * crealf (md) + cimagf (md) * cimagf (md));
        double lock_signal = 2.0 * (inst_pwr - mid_pwr)
                             / (inst_pwr + mid_pwr + SYMSYNC_LOCK_EPS);
        s->lock_sum += lock_signal;
        if (++s->lock_count >= s->avgs)
          {
            s->lock_stat = s->lock_sum / (double)s->avgs;
            (void)lockdet_step (&s->lock, s->lock_stat);
            s->lock_sum   = 0.0;
            s->lock_count = 0;
          }
        *y_out = y;
        emit   = 1;
      }
    else
      s->have_ontime = 1;
    s->prev_ontime = y;
    return emit;
  }

  void symsync_tlm_flush (const symsync_state_t *s);

  JM_FORCEINLINE JM_HOT int
  symsync_step (symsync_state_t *s, float complex x, float complex *y_out)
  {
    int r = symsync_step_ted (s, x, y_out, s->ted);
    if (r && s->tlm.ctx)
      symsync_tlm_flush (s);
    return r;
  }

  void symsync_init (symsync_state_t *s, size_t sps, double bn, double zeta,
                     int order, int ted);

  symsync_state_t *symsync_create (size_t sps, double bn, double zeta,
                                   int order, int ted);

  void symsync_destroy (symsync_state_t *state);

  void symsync_reset (symsync_state_t *state);

  size_t symsync_steps_max_out (symsync_state_t *state);
  size_t symsync_steps (symsync_state_t *state, const float complex *x,
                        size_t x_len, float complex *out, size_t max_out);
  void   symsync_configure (symsync_state_t *state, double bn, double zeta);
  double symsync_get_bn (const symsync_state_t *state);
  void   symsync_set_bn (symsync_state_t *state, double val);
  double symsync_get_timing_error (const symsync_state_t *state);
  double symsync_get_rate (const symsync_state_t *state);

  double symsync_get_lock_stat (const symsync_state_t *state);

  int symsync_get_locked (const symsync_state_t *state);

  int symsync_configure_lock (symsync_state_t *state, double rolloff,
                              double esno_min_db, double pfa, double pd);

  void symsync_configure_lock_raw (symsync_state_t *state, size_t avgs,
                                   double up_thresh, double down_thresh,
                                   uint32_t n_up, uint32_t n_down);

  int symsync_set_telemetry (symsync_state_t *state, dp_tlm_t *tlm,
                             const char *prefix, uint32_t decim);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * pointer-free composition: nco + farrow + loop_filter embedded by value
 * (all POD) + scalar timing state — a whole-struct snapshot. */
#define SYMSYNC_STATE_MAGIC DP_FOURCC ('S', 'Y', 'N', 'C')
#define SYMSYNC_STATE_VERSION                                                 \
  5u /* v5: block-averaged lock_signal statistic (avgs/lock_sum/lock_count)   \
      */
  size_t symsync_state_bytes (const symsync_state_t *state);
  void   symsync_get_state (const symsync_state_t *state, void *blob);
  int    symsync_set_state (symsync_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* SYMSYNC_CORE_H */
```


