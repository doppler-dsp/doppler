

# File carrier\_nda\_core.h

[**File List**](files.md) **>** [**carrier\_nda**](dir_425637d1941eacd8ae8cdd8750b207f0.md) **>** [**carrier\_nda\_core.h**](carrier__nda__core_8h.md)

[Go to the documentation of this file](carrier__nda__core_8h.md)


```C++

#ifndef CARRIER_NDA_CORE_H
#define CARRIER_NDA_CORE_H

#include "agc/agc_core.h"
#include "boxcar/boxcar_core.h"
#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "lo/lo_core.h"
#include "lockdet/lockdet_core.h"
#include "loop_filter/loop_filter_core.h"
#include "telemetry/telemetry.h"
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif

/* Numerical guard on the arm-sample magnitude (not tunable). */
#define CARRIER_NDA_EPS 1e-12
/* rad/sample -> cycles/sample for the NCO control port (replaces /(2*pi)). */
#define CARRIER_NDA_INV_2PI 0.15915494309189535 /* 1 / (2*pi) */
/* EMA smoothing for the lock metric (status diagnostic / handover input). */
#define CARRIER_NDA_LOCK_ALPHA 0.05
/* Arm AGC (the embedded log-domain agc_core primitive) — drives the
 * phase-detector input to unit average power so the loop gain is amplitude-
 * invariant. The AGC runs once per moving-average output and MUST stay slow
 * relative to the carrier loop: its bandwidth is locked to a fixed fraction of
 * the carrier loop bandwidth (agc.loop_bw = CARRIER_NDA_AGC_BW_RATIO * bn), so
 * it is always 100× slower and tracks only the overall signal level — never
 * the carrier dynamics or the within-symbol pulse (RRC) envelope. Flattening
 * the envelope would destroy the raw M-th-power discriminator's natural |z|^M
 * weighting and corrupt the phase estimate on pulse-shaped signals. */
#define CARRIER_NDA_AGC_REF_DB 0.0
#define CARRIER_NDA_AGC_BW_RATIO 0.01
#define CARRIER_NDA_AGC_ALPHA 0.01
/* Saturated-amplifier soft clip: the AGC's square clip set 10 dB above the
 * unit level. Bounds the peak (constructive-ISI) arm samples that would
 * otherwise dominate the |z|^M weighting, while constant-modulus samples sit
 * below it and pass through unclipped (keeping the raw-arm squaring-loss
 * advantage). */
#define CARRIER_NDA_AGC_CLIP_DB 10.0

  typedef struct
  {
    dp_tlm_t *ctx;       
    int32_t   id_lock;   
    int32_t   id_e;      
    int32_t   id_freq;   
    int32_t   id_locked; 
  } carrier_nda_tlm_t;

  typedef struct
  {
    lo_state_t          nco; 
    loop_filter_state_t lf;  
    size_t              sps; 
    int                 m;   
    int                 n;   
    size_t arm_len;          
    double lock_scale;       
    double seed_norm_freq;   
    double bn;               
    double zeta;             
    boxcar_state_t arm;      
    double         lock;     
    double         last_error; 
    agc_state_t    agc;        
    double          ctl_cyc; 
    lockdet_state_t lockdet; 
    carrier_nda_tlm_t tlm;   
  } carrier_nda_state_t;

  JM_FORCEINLINE void
  carrier_nda_disc (float complex z, int m, double scale, double *pe,
                    double *lock)
  {
    /* The cascade runs in float: the input is a float complex AGC-normalized
     * to |z|~1 (clip caps it at ~3.16), so even z^8 is O(1)-O(1e4) and float's
     * ~1e-7 relative error is far below what the loop tolerates. Keeping it in
     * float avoids the float->double conversions on this loop-carried critical
     * path; only the two outputs (which feed the double loop filter) promote.
     */
    float i  = crealf (z);    /* raw I (AGC-normalized upstream) */
    float q  = cimagf (z);    /* raw Q                          */
    float bl = i * i - q * q; /* Re(z^2) */
    float be = 2.0f * i * q;  /* Im(z^2) */
    if (m == 2)
      {
        *pe   = be;
        *lock = scale * bl;
        return;
      }
    float ql = bl * bl - be * be; /* Re(z^4)        */
    float qe = be * bl;           /* Im(z^4) / 2    */
    if (m == 4)
      {
        *pe   = qe;
        *lock = scale * ql;
        return;
      }
    *pe   = qe * ql;                     /* Im(z^8) / 4               */
    *lock = scale * (ql * ql - qe * qe); /* faithful 8-PSK lock det.  */
  }

  void carrier_nda_init (carrier_nda_state_t *s, double bn, double zeta,
                         double init_norm_freq, size_t sps, int n, int m);

  JM_FORCEINLINE JM_HOT float complex
  carrier_nda_wipeoff (carrier_nda_state_t *s, float complex x)
  {
    /* De-rotate through the NCO's control port: the LO advances by its centre
     * frequency (phase_inc) plus the loop's last control (ctl_cyc, set by
     * carrier_nda_steer). The LO owns the phase accumulation and scaling. */
    return x * conjf (lo_step_ctrl (&s->nco, s->ctl_cyc));
  }

  JM_FORCEINLINE JM_HOT int
  carrier_nda_arm_step (carrier_nda_state_t *s, float complex d, double *pe,
                        double *lock)
  {
    /* Slide the boxcar moving average by one sample (unit gain — pure I/Q
     * average), then normalize that window sample to unit average power with
     * the embedded AGC so the loop gain is amplitude-invariant (the role the
     * old per-sample |z| divide served, now as a slow feedback loop). agc_step
     * is the exact per-sample AGC — gain-apply, power detector, dB loop filter
     * and square clip in one call. The arm is in the *fast* carrier loop, so
     * the AGC runs per sample (no decimation, no block latency in the feedback
     * path); its own slowness (loop_bw = 0.01*bn, ~100x below the carrier
     * loop) is what keeps it tracking the overall level only — never the
     * carrier dynamics or the within-symbol pulse envelope. The square clip
     * (clip_db) saturates the peak (constructive-ISI) samples while
     * constant-modulus samples pass through, so the raw M-th-power
     * discriminator keeps its squaring-loss advantage. */
    float complex y  = boxcar_step (&s->arm, d);
    float complex zn = agc_step (&s->agc, y);
    carrier_nda_disc (zn, s->m, s->lock_scale, pe, lock);
    return 1;
  }

  JM_FORCEINLINE JM_HOT void
  carrier_nda_steer (carrier_nda_state_t *s, double pe)
  {
    s->last_error = pe;
    /* The PI loop filter output (integ + kp*pe) is the NCO frequency command.
     * config_loop folds the rad->cycle constant (1/2*pi) into kp/ki, so the
     * output is already in cycles/sample — store it directly as the control
     * the next wipeoff feeds to the LO's control port (no per-sample
     * conversion). The LO does the cycles->phase scaling and phase
     * accumulation, so the loop never touches the integer phase. The loop
     * filter is init'd with t = 1 (the MA arm updates every sample), so bn is
     * cycles/sample and n-invariant — n only sets the window length. lf.integ
     * is thus the carrier frequency correction in cycles/sample (read back by
     * carrier_nda_get_norm_freq). */
    s->ctl_cyc = loop_filter_step (&s->lf, pe);
  }

  carrier_nda_state_t *carrier_nda_create (double bn, double zeta,
                                           double init_norm_freq, size_t sps,
                                           int n, int m);

  void carrier_nda_destroy (carrier_nda_state_t *state);

  void carrier_nda_reset (carrier_nda_state_t *state);

  void carrier_nda_tlm_flush (const carrier_nda_state_t *s);

  int carrier_nda_set_telemetry (carrier_nda_state_t *state, dp_tlm_t *tlm,
                                 const char *prefix, uint32_t decim);

  void carrier_nda_configure_lock (carrier_nda_state_t *state,
                                   double up_thresh, double down_thresh,
                                   uint32_t n_up, uint32_t n_down);

  int carrier_nda_get_locked (const carrier_nda_state_t *state);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Pointer-free POD struct, so a whole-struct snapshot resumes the loop
 * exactly.
 */
#define CARRIER_NDA_STATE_MAGIC DP_FOURCC ('C', 'N', 'D', 'A')
#define CARRIER_NDA_STATE_VERSION                                             \
  4u /* v4: lockdet decision rule (verify counters) */

  size_t carrier_nda_state_bytes (const carrier_nda_state_t *state);
  void carrier_nda_get_state (const carrier_nda_state_t *state, void *blob);
  int carrier_nda_set_state (carrier_nda_state_t *state, const void *blob);

  size_t carrier_nda_steps_max_out (carrier_nda_state_t *state);
  size_t carrier_nda_steps (carrier_nda_state_t *state, const float complex *x,
                            size_t x_len, float complex *out, size_t max_out);
  double carrier_nda_get_norm_freq (const carrier_nda_state_t *state);
  void   carrier_nda_set_norm_freq (carrier_nda_state_t *state, double val);
  double carrier_nda_get_lock (const carrier_nda_state_t *state);
  double carrier_nda_get_last_error (const carrier_nda_state_t *state);
  double carrier_nda_get_bn (const carrier_nda_state_t *state);
  void   carrier_nda_set_bn (carrier_nda_state_t *state, double val);
  int    carrier_nda_get_m (const carrier_nda_state_t *state);
  int    carrier_nda_get_n (const carrier_nda_state_t *state);
  size_t carrier_nda_get_sps (const carrier_nda_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* CARRIER_NDA_CORE_H */
```


