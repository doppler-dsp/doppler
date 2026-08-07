

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
#include "dp_tlm/dp_tlm_core.h"
#include <math.h>
#include "telemetry/telemetry_core.h"
#ifdef __cplusplus
extern "C"
{
#endif

/* Numerical guard on the arm-sample magnitude (not tunable). */
#define CARRIER_NDA_EPS 1e-12
/* rad/sample -> cycles/sample for the NCO control port (replaces /(2*pi)). */
#define CARRIER_NDA_INV_2PI 0.15915494309189535 /* 1 / (2*pi) */
/* ── The lock statistic, and where its threshold comes from ──────────────
 *
 * `lock_signal = Re((z/|z|)^M)` -- the M-th power of a LIMITED sample. The
 * limiter is on this path only; the phase error keeps the raw |z|^M weighting,
 * which is deliberate (it is the natural matched weighting on a pulse-shaped
 * signal, and flattening it corrupts the phase estimate). Limiting the lock
 * signal is what makes it a detector you can put a number on:
 *
 *   - **Bounded.** Each look is Re(e^{j M theta}) in [-1, 1], so the EMA is too.
 *     The raw form is unbounded and, at M = 8, |z|^8 on Gaussian noise gives it
 *     an sd of 137 per look against a value of 1.0 at lock.
 *   - **M-independent.** Under H0 (no carrier) theta is uniform, so
 *     Var[Re(e^{j M theta})] = 1/2 for EVERY M. One threshold is one Pfa at
 *     every constellation order -- which is the property that makes a single
 *     `lock_thresh` mean one thing, and which the unlimited statistic does not
 *     have at any scaling.
 *   - **Detectable.** Measured d' = (mu_H1 - mu_H0)/sd_H0 post-EMA, raw vs
 *     limited, at Es/N0 = 10 / 20 dB: BPSK 5.70/6.21 -> 7.95/8.75,
 *     QPSK 1.50/1.78 -> 5.81/8.47, 8PSK 0.02/0.04 -> 1.76/7.52. The limiter
 *     costs H1 (it discards the |z|^M boost at low SNR) and wins anyway,
 *     everywhere, because it cuts H0's variance by far more than it cuts H1.
 *     With the raw form only BPSK ever clears a 1e-3 Pfa, so no Pfa-derived
 *     threshold existed for M >= 4 at all.
 *
 * The chain, all three numbers derived rather than picked:
 *
 *   alpha  = det_ema_alpha(0.0, 15.9 dB) = 0.05  -> N_eff = (2-a)/a = 39 looks
 *   sd_H0  = sqrt(1/2 * alpha/(2-alpha))  = 0.1132   (analytic; measured 0.1132)
 *   thresh = eta * sd_H0, eta from the Pfa budget
 *
 * `CARRIER_NDA_LOCK_DEFAULT_UP` = 0.5 is eta = 4.416, i.e. a per-look
 * Pfa of 5e-6 -- so the long-standing default turns out to BE the Pfa-derived
 * value once the statistic is M-independent. It was only ever meaningful at
 * BPSK before: the same 0.5 was eta = 0.9 at QPSK and eta = 0.02 at 8PSK.
 */
/* EMA smoothing for the lock metric: alpha = det_ema_alpha(0.0, 15.9), giving
 * N_eff = (2-alpha)/alpha = 39 effective looks (>= the 30-look floor). */
#define CARRIER_NDA_LOCK_ALPHA 0.05
/* Analytic H0 sd of the limited lock statistic AFTER the EMA above:
 * sqrt(Var_look * alpha/(2-alpha)) with Var_look = 1/2 exactly, for every M.
 * A threshold of `eta * this` has per-look Pfa = Q(eta). */
#define CARRIER_NDA_LOCK_NORM_SD 0.11322770341445956
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
  carrier_nda_disc (float complex z, int m, double *pe, double *lock)
  {
    /* The cascade runs in float: the input is a float complex AGC-normalized
     * to |z|~1 (clip caps it at ~3.16), so even z^8 is O(1)-O(1e4) and float's
     * ~1e-7 relative error is far below what the loop tolerates. Keeping it in
     * float avoids the float->double conversions on this loop-carried critical
     * path; only the two outputs (which feed the double loop filter) promote.
     */
    float i  = crealf (z);    /* raw I (AGC-normalized upstream) */
    float q  = cimagf (z);    /* raw Q                          */
    float p  = i * i + q * q; /* |z|^2                          */
    float bl = i * i - q * q; /* Re(z^2) */
    float be = 2.0f * i * q;  /* Im(z^2) */
    /* The LOCK signal is limited, the PHASE ERROR is not -- see the two
     * paragraphs below carrier_nda_lock_norm_sd. |z|^M is a power of p for
     * every M we support, so limiting costs one divide and no sqrt. */
    if (m == 2)
      {
        *pe   = be;
        *lock = (p > CARRIER_NDA_EPS) ? bl / p : 0.0; /* Re((z/|z|)^2) */
        return;
      }
    float ql = bl * bl - be * be; /* Re(z^4)        */
    float qe = be * bl;           /* Im(z^4) / 2    */
    if (m == 4)
      {
        *pe   = qe;
        *lock = (p > CARRIER_NDA_EPS) ? ql / (p * p) : 0.0; /* Re((z/|z|)^4) */
        return;
      }
    *pe = qe * ql; /* Im(z^8) / 4                             */
    /* Re(z^8) = Re(z^4)^2 - Im(z^4)^2, and `qe` is HALF of Im(z^4) -- that
     * half being the deliberate {1, 1/2, 1/4} phase-error scaling which
     * equalises the S-curve slope across M. So reconstructing Re(z^8) from it
     * needs the 2 squared back: ql*ql - (2*qe)^2. Without the 4 the statistic is
     * Re(z^4)^2 - Im(z^4)^2/4, which is NOT Re(z^8) and, unlike it, is not
     * zero-mean on noise -- E[Re(z^4)^2] = E[Im(z^4)^2] for circular noise, so
     * the shortfall leaves a positive residual of (3/4)E[Im(z^4)^2]. Measured
     * on unit-power complex Gaussian noise, 4e5 samples: mean +8.94 without
     * the 4, -0.11 with it (and bit-identical to Re(z^8) computed directly).
     * The value AT LOCK is +1.0000 either way, which is why this hid: it
     * corrupted only the noise-only tail, i.e. exactly the false-alarm
     * behaviour a lock detector is thresholded on. */
    float p4 = p * p * p * p; /* |z|^8 */
    *lock    = (p4 > CARRIER_NDA_EPS) ? (ql * ql - 4.0f * qe * qe) / p4
                                      : 0.0; /* Re((z/|z|)^8) */
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
    carrier_nda_disc (zn, s->m, pe, lock);
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
  double carrier_nda_get_nco_freq (const carrier_nda_state_t *state);
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


