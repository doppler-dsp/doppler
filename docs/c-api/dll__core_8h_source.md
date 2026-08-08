

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
#include "nco/nco_core.h"
#include "dp_tlm/dp_tlm_core.h"
#include <complex.h>
#include <math.h>
#include "detection/detection_core.h"
#include "telemetry/telemetry_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Numerical guard on the early+late envelope sum (not tunable). */
#define DLL_EPS 1e-12

/* Clamp on the discriminator output |e| before it reaches the loop filter.
 * The old magnitude-domain discriminator (|E|-|L|)/(|E|+|L|) was always
 * inherently bounded in [-1, 1] (|E|-|L| <= |E|+|L| identically); the
 * power-domain 0.5*(Ep-Lp)/Pp form is NOT -- a data transition landing
 * badly enough to collapse the prompt power Pp near zero (the lookback
 * failing to find a clean candidate that epoch, a real, reachable case,
 * not a hypothetical one -- observed directly while porting this design)
 * can blow e up arbitrarily, injecting a huge phase nudge that cascades
 * into a runaway. Clamping restores the old design's inherent safety
 * property without changing behaviour in the overwhelming common case
 * (|e| well under 1 at any reasonable lock). */
#define DLL_DISC_CLAMP 1.0

typedef struct {
    dp_tlm_t *ctx;     
    int32_t id_e;      
    int32_t id_rate;   
    int32_t id_lock;   
    int32_t id_locked; 
} dll_tlm_t;

typedef struct {
    loop_filter_state_t lf;  
    nco_state_t code_nco;    
    const uint8_t *code;     
    size_t sf;               
    size_t sps;              
    double inv_sps;          
    double inv_tsamps;       
    double inv_tsamps2;      
    double inv_tsamps_sf;    
    double spacing;          
    double chip_pos;         
    double code_rate;        
    double rate_aid;         
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
    /* ── segments>1 chunked output + one-epoch-deep lookback (heap-owned,
     *    length `segments`; NULL when segments==1 -- dll_init()'s embedded/
     *    borrowed path is always segments==1, so this never needs a
     *    deinit contract there, same lifecycle class as `code`/owns_code).
     *    This is the direct C port of the coupled-despreader
     *    prototype's `find_max_power()`/`get_window()` (also
     *    `docs/design/async-despreader-working-design.md`'s own reference
     *    pseudocode) -- see dll_steps_impl()'s segments>1 branch, which
     *    builds the SAME named artifacts (`sums`, `backward_sums`,
     *    `correlations`) in the same order so it can be checked directly
     *    against that Python function line for line. */
    float complex *chunk_p;         
    float complex *chunk_e;         
    float complex *chunk_l;         
    float complex *sums;            
    float complex *last_backward_p; 
    float complex *last_e;          
    float complex *last_l;          
    int have_prev_epoch;     
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
dll_replica(const dll_state_t *s, double c)
{
    double sfd2 = 2.0 * (double)s->sf;
    double p = fmod(c * 2.0 - 0.5, sfd2);
    if (p < 0.0)
        p += sfd2;
    size_t i = (size_t)p;
    double mu = p - (double)i;
    size_t j = (i + 1 >= (size_t)sfd2) ? 0 : i + 1;
    float v0 = dll_chip_sign(s->code[i >> 1]);
    float v1 = dll_chip_sign(s->code[j >> 1]);
    return (float)((1.0 - mu) * v0 + mu * v1);
}

/* Normalised -> u32 phase word: nco_norm_freq_to_inc() /
 * nco_norm_phase_to_word() (native/inc/nco/nco_core.h) are the ONE shared
 * primitive for this conversion, under two names so the call site says
 * whether it holds a rate or an angle -- do not grow a private copy here
 * (a prior copy of this exact formula existed under the name
 * dll_cycles_to_phase_delta() and has been consolidated away; see
 * nco_norm_fold_()'s own doc comment for why duplicates of this
 * conversion keep drifting). */

void dll_init(dll_state_t *s, const uint8_t *code, size_t code_len, size_t sps,
              double init_chip, double bn, double zeta, double spacing);

JM_FORCEINLINE double
dll_dwell_center_chip_pos(const dll_state_t *s)
{
    uint32_t mid = s->code_nco.phase + (s->code_nco.phase_inc >> 1);
    return ((double)mid / 4294967296.0) * (double)s->sf;
}

JM_FORCEINLINE JM_HOT int
dll_accumulate(dll_state_t *s, float complex d)
{
    double sfd = (double)s->sf;
    double cp = dll_dwell_center_chip_pos(s);
    double ce = cp + s->spacing;
    if (ce >= sfd)
        ce -= sfd;
    double cl = cp - s->spacing;
    if (cl < 0.0)
        cl += sfd;
    s->acc_p += d * dll_replica(s, cp);
    s->acc_e += d * dll_replica(s, ce);
    s->acc_l += d * dll_replica(s, cl);
    /* nco_step_u32_ovf() (native/inc/nco/nco_core.h) is the ONE shared
       primitive for "advance one sample, report whether it wrapped" --
       this used to be a private inline reimplementation of exactly that. */
    uint8_t carry;
    (void)nco_step_u32_ovf(&s->code_nco, &carry);
    s->chip_pos = ((double)s->code_nco.phase / 4294967296.0) * sfd;
    return carry;
}

JM_FORCEINLINE JM_HOT void
dll_lock_accumulate(dll_state_t *s, float complex d)
{
    double co = dll_dwell_center_chip_pos(s) + s->off_chips;
    if (co >= (double)s->sf)
        co -= (double)s->sf;
    s->acc_o += d * dll_replica(s, co);
}

void dll_lock_look(dll_state_t *s, double norm);

void dll_lock_epoch(dll_state_t *s);

JM_FORCEINLINE JM_HOT uint32_t
dll_steer_inc(const dll_state_t *s, double ctrl)
{
    return nco_norm_freq_to_inc(s->inv_tsamps * (1.0 + s->rate_aid) + ctrl);
}

JM_FORCEINLINE JM_HOT void
dll_update(dll_state_t *s)
{
    float me = cabsf(s->acc_e), ml = cabsf(s->acc_l), mp = cabsf(s->acc_p);
    double ep = (double)me * me, lp = (double)ml * ml, pp = (double)mp * mp;
    double e = 0.5 * (ep - lp) / (pp + DLL_EPS);
    if (e > DLL_DISC_CLAMP)
        e = DLL_DISC_CLAMP;
    else if (e < -DLL_DISC_CLAMP)
        e = -DLL_DISC_CLAMP;
    s->last_error = e;
    loop_filter_step(&s->lf, e);
    /* Pure control deviation: the integrator alone, PLUS the
       proportional term spread smoothly over the whole next period
       rather than kicked directly into `phase` (see the comment
       above) -- kp*e chips of total correction over sf*sps samples is
       kp*e/(sf*sf*sps) extra cycles per sample, the same total
       chip-domain correction the original double-accumulator design
       applied as `chip_pos += kp*e`. Neither term involves "1.0", and
       neither divides -- inv_tsamps/inv_tsamps_sf are precomputed once
       at construction (configure_geometry()), never here. */
    double ctrl = s->lf.integ * s->inv_tsamps + s->lf.kp * e * s->inv_tsamps_sf;
    s->code_rate = 1.0 + s->lf.integ; /* public ratio observable only */
    /* rate_aid (0 = off): a fixed carrier-aiding rate bias, scaled by the
       nominal per-sample rate so it sums into the sample-and-hold phase_inc
       as a continuous adjustment across the epoch, not a phase pulse. */
    s->code_nco.phase_inc = dll_steer_inc(s, ctrl);
}

dll_state_t *dll_create(const uint8_t *code, size_t code_len, size_t sps, double init_chip, double bn, double zeta, double spacing, size_t segments);

size_t dll_lookback_segments(size_t tsamps, double max_error_db);

void dll_destroy(dll_state_t *state);

void dll_reset(dll_state_t *state);

size_t dll_steps_max_out(dll_state_t *state);

size_t dll_steps(dll_state_t *state, const float complex *x, size_t x_len, float complex *out, size_t max_out);

void dll_configure(dll_state_t *state, double bn, double zeta);
double dll_get_bn(const dll_state_t *state);
void dll_set_bn(dll_state_t *state, double val);

void dll_set_rate_aid(dll_state_t *state, double rate_aid);
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
 * composition+field-wise: loop_filter child (POD-embedded) + embedded NCO
 * (POD) + running correlators/loop/lock state; borrowed `code` pointer
 * restored by create; the segments>1 chunk/lookback buffers (heap-owned,
 * pointers, NOT part of the whole-struct snapshot) are packed/restored
 * field-wise when segments > 1. */
#define DLL_STATE_MAGIC DP_FOURCC ('D','L','L',' ')
#define DLL_STATE_VERSION 7u /* v7: `rate_aid` carrier-aiding field added
                                (whole-struct snapshot, so the blob grew).
                                v6: `sums` scratch field added to the
                                struct (pure epoch-local scratch, not
                                serialized -- grows sizeof(dll_state_t)
                                regardless, so the version marks the
                                layout change) (v5: precomputed
                                inv_tsamps/inv_tsamps2/inv_tsamps_sf
                                fields added to the struct; v4: fixed-
                                point code_nco + segments>1 chunked
                                lookback buffers; see dll_core.c) */
size_t dll_state_bytes (const dll_state_t *state);
void dll_get_state (const dll_state_t *state, void *blob);
int dll_set_state (dll_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DLL_CORE_H */
```


