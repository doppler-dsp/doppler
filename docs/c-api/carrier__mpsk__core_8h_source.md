

# File carrier\_mpsk\_core.h

[**File List**](files.md) **>** [**carrier\_mpsk**](dir_bb3a0f9e61a286c66b69840ec2385900.md) **>** [**carrier\_mpsk\_core.h**](carrier__mpsk__core_8h.md)

[Go to the documentation of this file](carrier__mpsk__core_8h.md)


```C++

#ifndef DP_CARRIER_MPSK_CORE_H
#define DP_CARRIER_MPSK_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include "doppler/lo/lo_core.h"
#include "doppler/loop_filter/loop_filter_core.h"
#include "doppler/mpsk/mpsk_core.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Numerical guard on the prompt magnitude in the discriminator (not tunable). */
#define CARRIER_MPSK_EPS 1e-12f
/* EMA smoothing for the decision-aligned lock metric (status diagnostic). */
#define CARRIER_MPSK_LOCK_ALPHA 0.1

typedef struct {
    dp_lo_state_t nco;          
    dp_loop_filter_state_t lf;  
    size_t tsamps;           
    double seed_norm_freq;   
    double bn;               
    double zeta;             
    double bn_fll;           
    double k_fll;            
    int m;                   
    float _Complex acc;       
    size_t acc_n;            
    float _Complex prev;      
    double prev_abs;         
    int have_prev;           
    double lock_metric;      
    double last_error;       
} dp_carrier_mpsk_state_t;

void carrier_mpsk_init(dp_carrier_mpsk_state_t *s, double bn, double zeta,
                       double init_norm_freq, size_t tsamps, double bn_fll,
                       int m);

JM_FORCEINLINE JM_HOT float _Complex
carrier_mpsk_wipeoff(dp_carrier_mpsk_state_t *s, float _Complex x)
{
    return x * conjf(lo_step(&s->nco));
}

JM_FORCEINLINE JM_HOT void
carrier_mpsk_update(dp_carrier_mpsk_state_t *s, float _Complex P)
{
    float _Complex ahat;
    mpsk_slice(P, s->m, &ahat);          /* nearest unit constellation point */
    float _Complex d = P * conjf(ahat);   /* data-wiped prompt (carrier only) */
    double aP = (double)cabsf(P) + CARRIER_MPSK_EPS;
    double e = (double)cimagf(d) / aP;   /* sin(phase error) near lock */
    s->last_error = e;
    /* FLL assist: a cross-product frequency discriminator on the data-wiped
     * prompts has a far wider linear range than the phase discriminator, so it
     * pulls the frequency integrator onto a large/moving residual the bare PLL
     * cannot. Wiping by the decision conj(ahat) removes the M-PSK data phase,
     * so a symbol change between symbols does not corrupt the cross product. */
    if (s->k_fll > 0.0 && s->have_prev)
    {
        /* Im(conj(prev) * d): the carrier rotation between the two prompts. */
        float cross = crealf(s->prev) * cimagf(d) - cimagf(s->prev) * crealf(d);
        double freq_err = (double)cross / (aP * s->prev_abs);
        s->lf.integ += s->k_fll * freq_err;
    }
    s->prev = d;
    s->prev_abs = aP;
    s->have_prev = 1;
    dp_loop_filter_step(&s->lf, e);
    /* per-symbol freq estimate (rad/symbol) -> rad/sample -> cycles/sample */
    double car_w = s->lf.integ / (double)s->tsamps;
    dp_lo_set_norm_freq(&s->nco, car_w / (2.0 * M_PI));
    /* proportional phase nudge: kp*e radians -> cycles -> uint32 phase
     * delta, via the one shared primitive (a bare truncating cast here
     * is UB on a negative value -- see nco_norm_freq_to_inc()'s own doc). */
    s->nco.phase += nco_norm_phase_to_word ((s->lf.kp * e) / (2.0 * M_PI));
    /* lock metric: Re(P conj(ahat))/|P| EMA (1 = phase-locked, ~0 = no carrier) */
    double inst = (double)crealf(d) / aP;
    s->lock_metric += CARRIER_MPSK_LOCK_ALPHA * (inst - s->lock_metric);
}

dp_carrier_mpsk_state_t *dp_carrier_mpsk_create(double bn, double zeta, double init_norm_freq, size_t tsamps, double bn_fll, int m);

void dp_carrier_mpsk_destroy(dp_carrier_mpsk_state_t *state);

void dp_carrier_mpsk_reset(dp_carrier_mpsk_state_t *state);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Pointer-free POD struct, so a whole-struct snapshot resumes the loop exactly.
 */
#define CARRIER_MPSK_STATE_MAGIC DP_FOURCC('C', 'M', 'P', 'K')
#define CARRIER_MPSK_STATE_VERSION 1u

size_t dp_carrier_mpsk_state_bytes(const dp_carrier_mpsk_state_t *state);
void dp_carrier_mpsk_get_state(const dp_carrier_mpsk_state_t *state, void *blob);
int dp_carrier_mpsk_set_state(dp_carrier_mpsk_state_t *state, const void *blob);

size_t dp_carrier_mpsk_steps_max_out(dp_carrier_mpsk_state_t *state);

size_t dp_carrier_mpsk_steps(dp_carrier_mpsk_state_t *state, const float _Complex *x, size_t x_len, float _Complex *out, size_t max_out);

void dp_carrier_mpsk_configure(dp_carrier_mpsk_state_t *state, double bn, double zeta);
double dp_carrier_mpsk_get_bn(const dp_carrier_mpsk_state_t *state);
void dp_carrier_mpsk_set_bn(dp_carrier_mpsk_state_t *state, double val);
double dp_carrier_mpsk_get_norm_freq(const dp_carrier_mpsk_state_t *state);
void dp_carrier_mpsk_set_norm_freq(dp_carrier_mpsk_state_t *state, double val);
double dp_carrier_mpsk_get_lock_metric(const dp_carrier_mpsk_state_t *state);
double dp_carrier_mpsk_get_last_error(const dp_carrier_mpsk_state_t *state);
double dp_carrier_mpsk_get_bn_fll(const dp_carrier_mpsk_state_t *state);
void dp_carrier_mpsk_set_bn_fll(dp_carrier_mpsk_state_t *state, double val);
int dp_carrier_mpsk_get_m(const dp_carrier_mpsk_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* CARRIER_MPSK_CORE_H */
```


