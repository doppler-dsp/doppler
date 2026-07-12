

# File dll\_core.h

[**File List**](files.md) **>** [**dll**](dir_f3da3e2048ea3a8b9e723d3c5367d8f8.md) **>** [**dll\_core.h**](dll__core_8h.md)

[Go to the documentation of this file](dll__core_8h.md)


```C++

#ifndef DLL_CORE_H
#define DLL_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "lockdet/lockdet_core.h"
#include "loop_filter/loop_filter_core.h"
#include "telemetry/telemetry.h"
#include <complex.h>
#include "detection/detection_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Numerical guard on the early+late envelope sum (not tunable). */
#define DLL_EPS 1e-12

typedef struct {
    dp_tlm_t *ctx;     
    int32_t id_e;      
    int32_t id_rate;   
    int32_t id_lock;   
    int32_t id_locked; 
} dll_tlm_t;

typedef struct {
    loop_filter_state_t lf;  
    const uint8_t *code;     
    size_t sf;               
    size_t sps;              
    double inv_sps;          
    double spacing;          
    double chip_pos;         
    double code_rate;        
    double seed_chip;        
    double bn;               
    double zeta;             
    float complex acc_e;     
    float complex acc_p;     
    float complex acc_l;     
    double last_error;       
    size_t segments;         
    double seg_chips;        
    double seg_norm;         
    size_t seg_idx;          
    double sum_e;            
    double sum_l;            
    /* ── lock detector (always on): offset-tap CFAR noise ref + N-look test  */
    float complex acc_o;     
    double off_chips;        
    double noise_guard;      
    uint32_t rng;            
    double noise_ema;        
    double lock_alpha;       
    double lock_sum;         
    size_t lock_count;       
    size_t n_looks;          
    double lock_stat;        
    size_t lock_nz;          
    lockdet_state_t lock;    
    int owns_code;           
    dll_tlm_t tlm;           
} dll_state_t;

JM_FORCEINLINE float
dll_chip_sign(uint8_t c)
{
    return (c & 1u) ? -1.0f : 1.0f;
}

JM_FORCEINLINE float
dll_replica(const dll_state_t *s, double c, double adv)
{
    size_t i = (size_t)c;
    if (i >= s->sf)
        i = s->sf - 1;
    /* Distance from this sample to the next chip boundary. When it is at least
       one sample (adv), no transition falls inside the sample and the replica
       is a clean single chip sign — the common path, no divide. */
    double rem = (double)(i + 1) - c;
    if (rem >= adv)
        return dll_chip_sign(s->code[i]);
    /* Rare: the sample straddles the boundary; blend by the in-chip fraction. */
    double frac = rem / adv;
    size_t j = (i + 1 >= s->sf) ? 0 : i + 1; /* next chip, wraps the period */
    return (float)(frac * dll_chip_sign(s->code[i])
                   + (1.0 - frac) * dll_chip_sign(s->code[j]));
}

void dll_init(dll_state_t *s, const uint8_t *code, size_t code_len, size_t sps,
              double init_chip, double bn, double zeta, double spacing);

JM_FORCEINLINE JM_HOT void
dll_accumulate(dll_state_t *s, float complex d)
{
    double adv = s->code_rate * s->inv_sps;
    double cp = s->chip_pos;
    double sfd = (double)s->sf;
    double ce = cp + s->spacing;
    if (ce >= sfd)
        ce -= sfd;
    double cl = cp - s->spacing;
    if (cl < 0.0)
        cl += sfd;
    /* Fractional-boundary integrate-and-dump: each tap's replica blends across a
       chip transition that falls inside the sample (dll_replica), so the E/P/L
       correlations vary continuously with sub-sample code phase. */
    s->acc_p += d * dll_replica(s, cp, adv);
    s->acc_e += d * dll_replica(s, ce, adv);
    s->acc_l += d * dll_replica(s, cl, adv);
    s->chip_pos += adv;
}

JM_FORCEINLINE JM_HOT void
dll_lock_accumulate(dll_state_t *s, float complex d)
{
    double co = s->chip_pos + s->off_chips;
    if (co >= (double)s->sf)
        co -= (double)s->sf;
    s->acc_o += d * dll_replica(s, co, s->code_rate * s->inv_sps);
}

void dll_lock_look(dll_state_t *s, double norm);

void dll_lock_epoch(dll_state_t *s);

JM_FORCEINLINE JM_HOT void
dll_update(dll_state_t *s)
{
    float me = cabsf(s->acc_e), ml = cabsf(s->acc_l);
    double e = (double)(me - ml) / ((double)(me + ml) + DLL_EPS);
    s->last_error = e;
    loop_filter_step(&s->lf, e);
    s->code_rate = 1.0 + s->lf.integ;
    s->chip_pos -= (double)s->sf;
    s->chip_pos += s->lf.kp * e; /* proportional phase nudge, chips */
}

dll_state_t *dll_create(const uint8_t *code, size_t code_len, size_t sps, double init_chip, double bn, double zeta, double spacing, size_t segments);

void dll_destroy(dll_state_t *state);

void dll_reset(dll_state_t *state);

size_t dll_steps_max_out(dll_state_t *state);
size_t dll_steps(dll_state_t *state, const float complex *x, size_t x_len, float complex *out, size_t max_out);
void dll_configure(dll_state_t *state, double bn, double zeta);
double dll_get_bn(const dll_state_t *state);
void dll_set_bn(dll_state_t *state, double val);
double dll_get_code_phase(const dll_state_t *state);
double dll_get_code_rate(const dll_state_t *state);
double dll_get_last_error(const dll_state_t *state);
size_t dll_get_segments(const dll_state_t *state);

int dll_configure_lock(dll_state_t *state, double pfa, size_t n_looks, double ref_snr_db);

void dll_configure_lock_raw(dll_state_t *state, double up_thresh,
                            double down_thresh, size_t n_looks, double alpha,
                            uint32_t n_up, uint32_t n_down);

int dll_get_locked(const dll_state_t *state);

double dll_get_lock_stat(const dll_state_t *state);

double dll_get_noise_est(const dll_state_t *state);

void dll_tlm_flush(const dll_state_t *s);

int dll_set_telemetry(dll_state_t *state, dp_tlm_t * tlm, const char * prefix, uint32_t decim);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition+field-wise: loop_filter child (POD-embedded) + running
 * correlators/loop/lock state; borrowed `code` pointer restored by create. */
#define DLL_STATE_MAGIC DP_FOURCC ('D','L','L',' ')
#define DLL_STATE_VERSION 3u /* v3: lockdet decision rule (verify counters) */
size_t dll_state_bytes (const dll_state_t *state);
void dll_get_state (const dll_state_t *state, void *blob);
int dll_set_state (dll_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DLL_CORE_H */
```


