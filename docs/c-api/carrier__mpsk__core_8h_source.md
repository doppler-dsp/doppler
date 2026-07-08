

# File carrier\_mpsk\_core.h

[**File List**](files.md) **>** [**carrier\_mpsk**](dir_aac9a6642a6538588e08cd0551821cb3.md) **>** [**carrier\_mpsk\_core.h**](carrier__mpsk__core_8h.md)

[Go to the documentation of this file](carrier__mpsk__core_8h.md)


```C++

#ifndef CARRIER_MPSK_CORE_H
#define CARRIER_MPSK_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "lo/lo_core.h"
#include "loop_filter/loop_filter_core.h"
#include "mpsk/mpsk_core.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Numerical guard on the prompt magnitude in the discriminator (not tunable). */
#define CARRIER_MPSK_EPS 1e-12f
/* EMA smoothing for the decision-aligned lock metric (status diagnostic). */
#define CARRIER_MPSK_LOCK_ALPHA 0.1

typedef struct {
    lo_state_t nco;          
    loop_filter_state_t lf;  
    size_t tsamps;           
    double seed_norm_freq;   
    double bn;               
    double zeta;             
    double bn_fll;           
    double k_fll;            
    int m;                   
    float complex acc;       
    size_t acc_n;            
    float complex prev;      
    double prev_abs;         
    int have_prev;           
    double lock_metric;      
    double last_error;       
} carrier_mpsk_state_t;

void carrier_mpsk_init(carrier_mpsk_state_t *s, double bn, double zeta,
                       double init_norm_freq, size_t tsamps, double bn_fll,
                       int m);

JM_FORCEINLINE JM_HOT float complex
carrier_mpsk_wipeoff(carrier_mpsk_state_t *s, float complex x)
{
    return x * conjf(lo_step(&s->nco));
}

JM_FORCEINLINE JM_HOT void
carrier_mpsk_update(carrier_mpsk_state_t *s, float complex P)
{
    float complex ahat;
    mpsk_slice(P, s->m, &ahat);          /* nearest unit constellation point */
    float complex d = P * conjf(ahat);   /* data-wiped prompt (carrier only) */
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
    loop_filter_step(&s->lf, e);
    /* per-symbol freq estimate (rad/symbol) -> rad/sample -> cycles/sample */
    double car_w = s->lf.integ / (double)s->tsamps;
    lo_set_norm_freq(&s->nco, car_w / (2.0 * M_PI));
    /* proportional phase nudge: kp*e radians -> uint32 phase delta */
    s->nco.phase += (uint32_t)((s->lf.kp * e) / (2.0 * M_PI) * 4294967296.0);
    /* lock metric: Re(P conj(ahat))/|P| EMA (1 = phase-locked, ~0 = no carrier) */
    double inst = (double)crealf(d) / aP;
    s->lock_metric += CARRIER_MPSK_LOCK_ALPHA * (inst - s->lock_metric);
}

carrier_mpsk_state_t *carrier_mpsk_create(double bn, double zeta, double init_norm_freq, size_t tsamps, double bn_fll, int m);

void carrier_mpsk_destroy(carrier_mpsk_state_t *state);

void carrier_mpsk_reset(carrier_mpsk_state_t *state);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Pointer-free POD struct, so a whole-struct snapshot resumes the loop exactly.
 */
#define CARRIER_MPSK_STATE_MAGIC DP_FOURCC('C', 'M', 'P', 'K')
#define CARRIER_MPSK_STATE_VERSION 1u

size_t carrier_mpsk_state_bytes(const carrier_mpsk_state_t *state);
void carrier_mpsk_get_state(const carrier_mpsk_state_t *state, void *blob);
int carrier_mpsk_set_state(carrier_mpsk_state_t *state, const void *blob);

size_t carrier_mpsk_steps_max_out(carrier_mpsk_state_t *state);
size_t carrier_mpsk_steps(carrier_mpsk_state_t *state, const float complex *x, size_t x_len, float complex *out, size_t max_out);
void carrier_mpsk_configure(carrier_mpsk_state_t *state, double bn, double zeta);
double carrier_mpsk_get_bn(const carrier_mpsk_state_t *state);
void carrier_mpsk_set_bn(carrier_mpsk_state_t *state, double val);
double carrier_mpsk_get_norm_freq(const carrier_mpsk_state_t *state);
void carrier_mpsk_set_norm_freq(carrier_mpsk_state_t *state, double val);
double carrier_mpsk_get_lock_metric(const carrier_mpsk_state_t *state);
double carrier_mpsk_get_last_error(const carrier_mpsk_state_t *state);
double carrier_mpsk_get_bn_fll(const carrier_mpsk_state_t *state);
void carrier_mpsk_set_bn_fll(carrier_mpsk_state_t *state, double val);
int carrier_mpsk_get_m(const carrier_mpsk_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* CARRIER_MPSK_CORE_H */
```


