

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
#include "loop_filter/loop_filter_core.h"
#include "nco/nco_core.h"
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    nco_state_t         timing; 
    farrow_state_t      farrow; 
    loop_filter_state_t lf;     
    size_t              sps;    
    uint32_t      base_inc;     
    int           have_ontime;  
    float complex prev_ontime;  
    float complex mid;          
    double        bn;           
    double        zeta;         
    double        last_error;   
    double        rate_est;     
    double        pwr_avg;      
  } symsync_state_t;

  JM_FORCEINLINE JM_HOT int
  symsync_step (symsync_state_t *s, float complex x, float complex *y_out)
  {
    const uint32_t HALF = 0x80000000u;
    farrow_push (&s->farrow, x);
    uint32_t old   = s->timing.phase;
    uint64_t sum   = (uint64_t)old + s->timing.phase_inc;
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
    float         mu = (float)(1.0 - (double)s->timing.phase / inc);
    float complex y  = farrow_eval (&s->farrow, mu);
    int           emit = 0;
    if (s->have_ontime)
      {
        float complex diff = y - s->prev_ontime;
        double        num  = (double)(crealf (s->mid) * crealf (diff)
                                      + cimagf (s->mid) * cimagf (diff));
        double inst_pwr
            = (double)(crealf (y) * crealf (y) + cimagf (y) * cimagf (y));
        s->pwr_avg += 0.01 * (inst_pwr - s->pwr_avg);
        double e          = num / (s->pwr_avg + 1e-6);
        s->last_error     = e;
        double control    = loop_filter_step (&s->lf, e);
        s->timing.phase_inc
            = (uint32_t)((double)s->base_inc * (1.0 + control));
        double inst = (double)s->sps / (1.0 + control);
        double lo_r = 0.5 * (double)s->sps, hi_r = 1.5 * (double)s->sps;
        if (inst < lo_r)
          inst = lo_r;
        else if (inst > hi_r)
          inst = hi_r;
        s->rate_est += 0.02 * (inst - s->rate_est);
        *y_out = y;
        emit   = 1;
      }
    else
      s->have_ontime = 1;
    s->prev_ontime = y;
    return emit;
  }

  void symsync_init (symsync_state_t *s, size_t sps, double bn, double zeta,
                     int order);

  symsync_state_t *symsync_create (size_t sps, double bn, double zeta,
                                   int order);

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
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * pointer-free composition: nco + farrow + loop_filter embedded by value
 * (all POD) + scalar timing state — a whole-struct snapshot. */
#define SYMSYNC_STATE_MAGIC DP_FOURCC ('S','Y','N','C')
#define SYMSYNC_STATE_VERSION 1u
size_t symsync_state_bytes (const symsync_state_t *state);
void symsync_get_state (const symsync_state_t *state, void *blob);
int symsync_set_state (symsync_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* SYMSYNC_CORE_H */
```


