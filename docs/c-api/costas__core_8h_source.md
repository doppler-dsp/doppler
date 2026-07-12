

# File costas\_core.h

[**File List**](files.md) **>** [**costas**](dir_9b517cb2745356d7938c9e100210a101.md) **>** [**costas\_core.h**](costas__core_8h.md)

[Go to the documentation of this file](costas__core_8h.md)


```C++

#ifndef COSTAS_CORE_H
#define COSTAS_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "lo/lo_core.h"
#include "lockdet/lockdet_core.h"
#include "loop_filter/loop_filter_core.h"
#include "telemetry/telemetry.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Numerical guard on the prompt magnitude in the discriminator (not tunable). */
#define COSTAS_EPS 1e-12f
/* EMA smoothing for the |Re P|/|P| lock metric (status diagnostic). */
#define COSTAS_LOCK_ALPHA 0.1

typedef struct {
    dp_tlm_t *ctx;     
    int32_t id_lock;   
    int32_t id_e;      
    int32_t id_freq;   
    int32_t id_locked; 
} costas_tlm_t;

typedef struct {
    lo_state_t nco;          
    loop_filter_state_t lf;  
    size_t tsamps;           
    double seed_norm_freq;   
    double bn;               
    double zeta;             
    double bn_fll;           
    double k_fll;            
    float complex acc;       
    size_t acc_n;            
    float complex prev;      
    int have_prev;           
    double lock_metric;      
    lockdet_state_t lock;    
    double last_error;       
    costas_tlm_t tlm;        
} costas_state_t;

void costas_init(costas_state_t *s, double bn, double zeta,
                 double init_norm_freq, size_t tsamps, double bn_fll);

JM_FORCEINLINE JM_HOT float complex
costas_wipeoff(costas_state_t *s, float complex x)
{
    return x * conjf(lo_step(&s->nco));
}

JM_FORCEINLINE JM_HOT void
costas_update(costas_state_t *s, float complex P)
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
    loop_filter_step(&s->lf, e);
    /* per-symbol freq estimate (rad/symbol) -> rad/sample -> cycles/sample */
    double car_w = s->lf.integ / (double)s->tsamps;
    lo_set_norm_freq(&s->nco, car_w / (2.0 * M_PI));
    /* proportional phase nudge: kp*e radians -> uint32 phase delta */
    s->nco.phase += (uint32_t)((s->lf.kp * e) / (2.0 * M_PI) * 4294967296.0);
    /* lock metric: |Re|/|P| EMA (1 = phase-locked BPSK, ~0 = no carrier) */
    double inst = (double)(fabsf(reP) / aP);
    s->lock_metric += COSTAS_LOCK_ALPHA * (inst - s->lock_metric);
    /* verify-counted decision on the smoothed metric (lockdet_core.h):
     * hysteresis keeps a metric grazing the threshold from chattering
     * `locked`. Inline POD step — no call, one branch per symbol. */
    (void)lockdet_step(&s->lock, s->lock_metric);
}

costas_state_t *costas_create(double bn, double zeta, double init_norm_freq, size_t tsamps, double bn_fll);

void costas_destroy(costas_state_t *state);

void costas_reset(costas_state_t *state);

void costas_tlm_flush(const costas_state_t *s);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Pointer-free POD struct (embedded NCO + loop filter + I&D accumulators), so
 * a whole-struct snapshot resumes the loop exactly. */
#define COSTAS_STATE_MAGIC DP_FOURCC('C', 'S', 'T', 'S')
#define COSTAS_STATE_VERSION 3u /* v3: lockdet decision rule */

size_t costas_state_bytes(const costas_state_t *state);
void costas_get_state(const costas_state_t *state, void *blob);
int costas_set_state(costas_state_t *state, const void *blob);

size_t costas_steps_max_out(costas_state_t *state);
size_t costas_steps(costas_state_t *state, const float complex *x, size_t x_len, float complex *out, size_t max_out);
void costas_configure(costas_state_t *state, double bn, double zeta);
double costas_get_bn(const costas_state_t *state);
void costas_set_bn(costas_state_t *state, double val);
double costas_get_norm_freq(const costas_state_t *state);
void costas_set_norm_freq(costas_state_t *state, double val);
double costas_get_lock_metric(const costas_state_t *state);
double costas_get_last_error(const costas_state_t *state);
double costas_get_bn_fll(const costas_state_t *state);
void costas_set_bn_fll(costas_state_t *state, double val);

void costas_configure_lock(costas_state_t *state, double up_thresh,
                           double down_thresh, uint32_t n_up,
                           uint32_t n_down);

int costas_get_locked(const costas_state_t *state);

int costas_set_telemetry(costas_state_t *state, dp_tlm_t * tlm, const char * prefix, uint32_t decim);
#ifdef __cplusplus
}
#endif

#endif /* COSTAS_CORE_H */
```


