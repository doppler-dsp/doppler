

# File carrier\_nda\_core.h

[**File List**](files.md) **>** [**carrier\_nda**](dir_425637d1941eacd8ae8cdd8750b207f0.md) **>** [**carrier\_nda\_core.h**](carrier__nda__core_8h.md)

[Go to the documentation of this file](carrier__nda__core_8h.md)


```C++

#ifndef CARRIER_NDA_CORE_H
#define CARRIER_NDA_CORE_H

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
 * `lock_signal = Re((z/|z|)^M)` -- the M-th power of a LIMITED sample. Both
 * outputs are limited now (the phase error kept the raw |z|^M weighting until
 * the detector was made to normalise by its own amplitude law), but the two
 * paths wanted it for different reasons: the phase error to keep the loop
 * gain out of the input's hands, the lock signal to be a detector you can put
 * a number on:
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
    double          ctl_cyc; 
    lockdet_state_t lockdet; 
    carrier_nda_tlm_t tlm;   
  } carrier_nda_state_t;


  JM_FORCEINLINE void
  carrier_nda_disc (float complex z, int m, double *pe, double *lock)
  {
    /* BOTH outputs normalise by the detector's OWN amplitude law, |z|^M.
     *
     * A discriminator's raw output is the phase error multiplied by things it
     * did not choose, and amplitude is the largest of them: Im(z^M) scales as
     * A^M, so a 2x level error is 4x loop gain at BPSK and 256x at 8PSK. Only
     * the detector can divide that out, and it can do it exactly -- |z|^M is a
     * power of p for every M supported here, so it costs one divide and no
     * sqrt. This is the same rule the timing detector follows (a TED
     * normalises by its own slope, symsync_ted_slope()), applied to its
     * sibling.
     *
     * At |z| = 1 this is identical to the un-normalised form, so the S-curve
     * slope -- and with it the meaning of bn -- is unchanged from when an
     * upstream AGC was manufacturing that condition. What changes is that it
     * no longer HAS to be manufactured: an AGC ahead of this detector existed
     * only to make |z| = 1 true, and a receiver now needs exactly one AGC,
     * for its own signal path, not one per detector.
     *
     * DIVIDE ONCE, AT THE FIRST SQUARING -- not once per M at the end. The
     * pair (Re(z^2), Im(z^2))/p IS the unit vector (z/|z|)^2, and every later
     * squaring of a unit vector is a unit vector, so after that one divide
     * nothing in this function ever exceeds 1 in magnitude. Dividing at the
     * end instead means forming |z|^M explicitly, and BOTH ends of that
     * overflow the float range the AGC used to keep us away from: measured on
     * the |z|^8 form, M = 8 returned exactly 0 below |z| = 0.032 (the eps
     * guard, applied to |z|^8, trips at 1e-12^(1/8)) and NaN at |z| = 1e5
     * (|z|^8 = 1e40 > FLT_MAX, then inf/inf) -- and a NaN here poisons the
     * loop filter and the NCO permanently. With the divide hoisted, the guard
     * is on p alone and means the same thing at every M, and the outputs are
     * scale-invariant from 1e-5 to 1e15 (max 4.6e-7 relative). At |z| = 1 the
     * two forms agree to 1.8e-7 over a full phase sweep, so the S-curve slope
     * -- and with it the meaning of bn -- is unchanged either way.
     *
     * The cascade runs in float: the unit-magnitude intermediates are all
     * O(1) and float's ~1e-7 relative error is far below what the loop
     * tolerates. Keeping it in float avoids the float->double conversions on
     * this loop-carried critical path; only the two outputs (which feed the
     * double loop filter) promote. */
    float i = crealf (z);    /* raw I, any scale */
    float q = cimagf (z);    /* raw Q            */
    float p = i * i + q * q; /* |z|^2            */
    /* Written !(p > eps) so a NaN input yields zero rather than a NaN error
       fed to the loop filter. */
    if (!(p > CARRIER_NDA_EPS))
      {
        *pe = *lock = 0.0;
        return;
      }
    float rp = 1.0f / p;
    float bl = (i * i - q * q) * rp; /* Re((z/|z|)^2) */
    float be = (2.0f * i * q) * rp;  /* Im((z/|z|)^2) */
    if (m == 2)
      {
        *pe   = be; /* Im((z/|z|)^2) */
        *lock = bl; /* Re((z/|z|)^2) */
        return;
      }
    float ql = bl * bl - be * be; /* Re((z/|z|)^4)     */
    float qe = be * bl;           /* Im((z/|z|)^4) / 2 */
    if (m == 4)
      {
        *pe   = qe;
        *lock = ql;
        return;
      }
    /* Im((z/|z|)^8) / 4. */
    float pe8 = qe * ql;
    /* Re(u^8) = Re(u^4)^2 - Im(u^4)^2, and `qe` is HALF of Im(u^4) -- that
     * half being the deliberate {1, 1/2, 1/4} phase-error scaling which
     * equalises the S-curve slope across M. So reconstructing Re(u^8) from it
     * needs the 2 squared back: ql*ql - (2*qe)^2. Without the 4 the statistic
     * is Re(u^4)^2 - Im(u^4)^2/4, which is NOT Re(u^8) and, unlike it, is not
     * zero-mean on noise -- E[Re(u^4)^2] = E[Im(u^4)^2] for circular noise, so
     * the shortfall leaves a positive residual of (3/4)E[Im(u^4)^2]. Measured
     * on unit-power complex Gaussian noise, 4e5 samples: mean +8.94 without
     * the 4, -0.11 with it (and bit-identical to Re(z^8) computed directly).
     * The value AT LOCK is +1.0000 either way, which is why this hid: it
     * corrupted only the noise-only tail, i.e. exactly the false-alarm
     * behaviour a lock detector is thresholded on. */
    *pe   = pe8;                      /* Im((z/|z|)^8) / 4 */
    *lock = ql * ql - 4.0f * qe * qe; /* Re((z/|z|)^8)     */
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
     * average) and discriminate it directly. There is no AGC on this path and
     * none is wanted: carrier_nda_disc normalises by its own amplitude law,
     * so the loop gain is already amplitude-invariant and a second level loop
     * in series would only add its own transient to correct. */
    carrier_nda_disc (boxcar_step (&s->arm, d), s->m, pe, lock);
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
  5u /* v5: the arm AGC is gone -- carrier_nda_disc normalises by its own    \
        |z|^M, so nothing upstream has to manufacture |z| = 1 (gh-657) */

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


