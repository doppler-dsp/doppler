

# File costas\_core.h

[**File List**](files.md) **>** [**costas**](dir_8ebd78c7800b34d5dee6ef27ff63e7b3.md) **>** [**costas\_core.h**](costas__core_8h.md)

[Go to the documentation of this file](costas__core_8h.md)


```C++

#ifndef DP_COSTAS_CORE_H
#define DP_COSTAS_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include "doppler/lo/lo_core.h"
#include "doppler/lockdet/lockdet_core.h"
#include "doppler/loop_filter/loop_filter_core.h"
#include "doppler/state_filter/state_filter_core.h"
#include "doppler/dp_tlm/dp_tlm_core.h"
#include <math.h>
#include "doppler/telemetry/telemetry_core.h"
#ifdef __cplusplus
extern "C" {
#endif

#define COSTAS_EPS 1e-12f
#define COSTAS_LOCK_ALPHA 0.1

typedef struct {
    dp_tlm_t *ctx;     
    int32_t id_lock;   
    int32_t id_e;      
    int32_t id_freq;   
    int32_t id_locked; 
    int32_t id_nco;    
    int32_t id_freq_rate; 
    int32_t id_order;  
} costas_tlm_t;

typedef struct {
    dp_lo_state_t nco;          
    dp_loop_filter_state_t lf;  
    size_t tsamps;           
    double seed_norm_freq;   
    double bn;               
    double zeta;             
    double bn_fll;           
    double k_fll;            
    float _Complex acc;       
    size_t acc_n;            
    float _Complex prev;      
    int have_prev;           
    int32_t _pad;
    double lock_metric;      
    dp_lockdet_state_t lock;    
    double last_error;       
    costas_tlm_t tlm;        
    /* The third-order filter and the order sit LAST, after every field the
       per-sample loop touches (nco, tsamps, acc, acc_n): appended here they
       leave each of those at the offset it had before order 3 existed, so
       the order-2 loop's memory layout is the one it always had. */
    dp_state_filter_state_t sf; 
    int32_t order;              
    int32_t _pad_order;         
} dp_costas_state_t;

void dp_costas_init(dp_costas_state_t *s, double bn, double zeta,
                 double init_norm_freq, size_t tsamps, double bn_fll);

#ifndef __cplusplus
JM_FORCEINLINE JM_HOT float _Complex
costas_wipeoff(dp_costas_state_t *s, float _Complex x)
{
    return x * conjf(lo_step(&s->nco));
}

JM_FORCEINLINE JM_HOT void
costas_update(dp_costas_state_t *s, float _Complex P)
{
    float reP = crealf(P), imP = cimagf(P);
    float aP = cabsf(P) + COSTAS_EPS;
    double e = (double)(((reP >= 0.0f) ? imP : -imP) / aP);
    s->last_error = e;
    /* FLL assist: a decision-directed cross-product frequency discriminator
     * has a far wider linear range than the phase discriminator, so it pulls
     * the loop's frequency integrator onto a large/moving residual the bare
     * PLL cannot. Both prompts are data-wiped (multiplied by their Re sign)
     * so a BPSK bit flip between symbols does not corrupt the cross product.
     * The result (~Delta-phase per symbol, rad) nudges integ directly. */
    if (s->k_fll > 0.0 && s->have_prev)
    {
        float rpr = crealf(s->prev), ipr = cimagf(s->prev);
        float sc = (reP >= 0.0f) ? 1.0f : -1.0f;
        float sp = (rpr >= 0.0f) ? 1.0f : -1.0f;
        float ic = reP * sc, qc = imP * sc;       /* data-wiped current */
        float ip = rpr * sp, qp = ipr * sp;       /* data-wiped previous */
        float cross = ip * qc - qp * ic;          /* Im(conj(prev)*cur) */
        float apr = cabsf(s->prev) + COSTAS_EPS;
        double freq_err = (double)cross / ((double)aP * (double)apr);
        s->lf.integ += s->k_fll * freq_err;
    }
    s->prev = P;
    s->have_prev = 1;
    /* The active filter (dp_costas_set_order). The FLL above writes lf only:
     * order 3 refuses bn_fll > 0, so k_fll is 0 there. */
    double integ, kp;
    if (s->order == 3)
    {
        (void)dp_state_filter_step(&s->sf, e);
        integ = s->sf.integ;
        kp    = s->sf.kp;
    }
    else
    {
        dp_loop_filter_step(&s->lf, e);
        integ = s->lf.integ;
        kp    = s->lf.kp;
    }
    /* per-symbol freq estimate (rad/symbol) -> rad/sample -> cycles/sample */
    double car_w = integ / (double)s->tsamps;
    dp_lo_set_norm_freq(&s->nco, car_w / (2.0 * M_PI));
    /* proportional phase nudge: kp*e radians -> cycles -> uint32 phase
     * delta, via the one shared primitive (a bare truncating cast here
     * is UB on a negative value -- see nco_norm_freq_to_inc()'s own doc). */
    s->nco.phase += nco_norm_phase_to_word ((kp * e) / (2.0 * M_PI));
    /* lock metric: |Re|/|P| EMA (1 = phase-locked BPSK, ~0 = no carrier) */
    double inst = (double)(fabsf(reP) / aP);
    s->lock_metric += COSTAS_LOCK_ALPHA * (inst - s->lock_metric);
    /* verify-counted decision on the smoothed metric (lockdet_core.h):
     * hysteresis keeps a metric grazing the threshold from chattering
     * `locked`. Inline POD step — no call, one branch per symbol. */
    (void)dp_lockdet_step(&s->lock, s->lock_metric);
}
#endif /* !__cplusplus */

dp_costas_state_t *dp_costas_create(double bn, double zeta, double init_norm_freq, size_t tsamps, double bn_fll);

void dp_costas_destroy(dp_costas_state_t *state);

void dp_costas_reset(dp_costas_state_t *state);

void dp_costas_tlm_flush(const dp_costas_state_t *s);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Pointer-free POD struct (embedded NCO + loop filter + I&D accumulators), so
 * a whole-struct snapshot resumes the loop exactly. */
#define COSTAS_STATE_MAGIC DP_FOURCC('C', 'S', 'T', 'S')
#define COSTAS_STATE_VERSION 4u /* v4: sf + order */

size_t dp_costas_state_bytes(const dp_costas_state_t *state);
void dp_costas_get_state(const dp_costas_state_t *state, void *blob);
int dp_costas_set_state(dp_costas_state_t *state, const void *blob);

size_t dp_costas_steps_max_out(dp_costas_state_t *state);

size_t dp_costas_steps(dp_costas_state_t *state, const float _Complex *x, size_t x_len, float _Complex *out, size_t max_out);

int dp_costas_configure(dp_costas_state_t *state, double bn, double zeta);
double dp_costas_get_bn(const dp_costas_state_t *state);
int dp_costas_set_bn(dp_costas_state_t *state, double val);
double dp_costas_get_norm_freq(const dp_costas_state_t *state);
double dp_costas_get_nco_freq(const dp_costas_state_t *state);

double dp_costas_get_nco_phase_ahead(const dp_costas_state_t *state,
                                     size_t ahead);
void dp_costas_set_norm_freq(dp_costas_state_t *state, double val);
double dp_costas_get_lock_metric(const dp_costas_state_t *state);
double dp_costas_get_last_error(const dp_costas_state_t *state);
double dp_costas_get_bn_fll(const dp_costas_state_t *state);
int dp_costas_set_bn_fll(dp_costas_state_t *state, double val);

int dp_costas_set_order(dp_costas_state_t *state, int order, double bn);

int dp_costas_get_order(const dp_costas_state_t *state);

int dp_costas_set_norm_rate(dp_costas_state_t *state, double val);

double dp_costas_get_norm_rate(const dp_costas_state_t *state);

void dp_costas_configure_lock(dp_costas_state_t *state, double up_thresh,
                           double down_thresh, uint32_t n_up,
                           uint32_t n_down);

int dp_costas_get_locked(const dp_costas_state_t *state);

dp_lockdet_rule_t dp_costas_get_lock_rule(const dp_costas_state_t *state);

int dp_costas_set_telemetry(dp_costas_state_t *state, dp_tlm_t * tlm, const char * prefix, uint32_t decim);
#ifdef __cplusplus
}
#endif

#endif /* COSTAS_CORE_H */
```


