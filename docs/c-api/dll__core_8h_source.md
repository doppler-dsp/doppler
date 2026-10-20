

# File dll\_core.h

[**File List**](files.md) **>** [**dll**](dir_c5ff741ba8e1e68126223ee4df379547.md) **>** [**dll\_core.h**](dll__core_8h.md)

[Go to the documentation of this file](dll__core_8h.md)


```C++

#ifndef DP_DLL_CORE_H
#define DP_DLL_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include "doppler/lockdet/lockdet_core.h"
#include "doppler/loop_filter/loop_filter_core.h"
#include "doppler/nco/nco_core.h"
#include "doppler/dp_tlm/dp_tlm_core.h"
#include "doppler/dp_complex.h"
#include <math.h>
#include "doppler/detection/detection_core.h"
#include "doppler/telemetry/telemetry_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Numerical guard on the early+late envelope sum (not tunable). */
#define DLL_EPS 1e-12

/* The `segments > 1` output's level: the code periods its running mean of
 * the look-back magnitude spans (doppler#2495). Each period's partials are
 * divided by that slowly tracked level, so the output sits near unit
 * amplitude at any input gain -- the level every timing detector
 * downstream assumes, which MpskReceiver's AGC, 20x slower than its own
 * loops, cannot set from scratch -- and never by the period's own
 * magnitude, whose noise is the period's own. The number is a choice
 * inside a band, both ends set by a measurement or a count:
 *   - slow enough: the current period enters the level with weight 1/N,
 *     and the in-phase error is compressed against the quadrature as that
 *     weight grows. Measured by test_dll_core.c's section 11 harness (a
 *     period SNR of 3 dB, 20,000 periods), the compression is -2.29 dB at
 *     N = 1 (the period's own magnitude, which cost the receiver 0.2 dB of
 *     error rate), -1.18 at 2, -0.54 at 4, -0.23 at 8, -0.08 at 16 and
 *     -0.01 at 32: about halving with each doubling. Holding it under
 *     0.15 dB, about the counting noise of that measurement, needs
 *     N >= 16.
 *   - fast enough: after a step in level the error decays as (1 - 1/N)^n,
 *     to 1% at n = ln 100 / -ln(1 - 1/N), about 4.6 N periods. Settling
 *     inside a third of the 185 ms data-free block (555 periods at SPEC's
 *     3.069 Mcps), and far ahead of MpskReceiver's AGC, needs N <= 40.
 * N = 32 is inside the band 16..40, a power of two; the 1% settle is then
 * 146 periods, 49 ms at SPEC. Until N periods have accrued the level is
 * their plain mean, so the first output already carries a level, but not
 * a slow one: the first period is divided by its own magnitude, and the
 * current period weighs 1/n, so the compression the band bounds holds
 * only once about 16 periods have accrued, 5 ms at SPEC. */
#define DLL_OUT_LEVEL_PERIODS 32u

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
    int32_t id_nco;    
} dll_tlm_t;

typedef struct {
    dp_loop_filter_state_t lf;  
    dp_nco_state_t code_nco;    
    const uint8_t *code;     
    size_t sf;               
    size_t sps;              
    double inv_sps;          
    double inv_tsamps;       
    double inv_tsamps2;      
    double inv_tsamps_sf;    
    double inv_upd;          
    double spacing;          
    double chip_pos;         
    double code_rate;        
    double rate_aid;         
    int coast;               
    uint32_t held_inc;       
    dp_loop_filter_state_t held_lf; 
    /* The steer's gain table: how the loop filter's integrator and
       proportional term reach phase_inc (cycles per sample) and code_rate
       (a ratio), set once by segments in set_segments(). ONE steer,
       dll_steer(), applies it on both paths; the two tables record the
       measured, separately pinned gains each path shipped with (the
       coherent full-epoch loop treats the integrator as a rate ratio,
       the partial-correlation loop treats the filter's output as chips
       over sps) -- reconciling them is a certified-behaviour change,
       tracked, not folded in here. */
    double ctrl_i;           
    double ctrl_p;           
    double rate_i;           
    double rate_p;           
    double   err_sum;        
    uint64_t err_n;          
    double seed_chip;        
    double bn;               
    double zeta;             
    float _Complex acc_e;     
    float _Complex acc_p;     
    float _Complex acc_l;     
    double last_error;       
    size_t segments;         
    double seg_chips;        
    double seg_norm;         
    size_t seg_idx;          
    int wrap_pending;        
    int wrap_await;          
    /* ── segments>1 chunked output + one-epoch-deep lookback (heap-owned,
     *    length `segments`; NULL when segments==1 -- dp_dll_init()'s embedded/
     *    borrowed path is always segments==1, so this never needs a
     *    deinit contract there, same lifecycle class as `code`/owns_code).
     *    This is the direct C port of the coupled-despreader
     *    prototype's `find_max_power()`/`get_window()` (also
     *    `docs/design/async-dsss-receiver.md` §3.6's own reference
     *    pseudocode) -- see dll_steps_impl()'s segments>1 branch, which
     *    builds the SAME named artifacts (`sums`, `backward_sums`,
     *    `correlations`) in the same order so it can be checked directly
     *    against that Python function line for line. */
    float _Complex *chunk_p;         
    float _Complex *chunk_e;         
    float _Complex *chunk_l;         
    float _Complex *sums;            
    float _Complex *last_backward_p; 
    float _Complex *last_e;          
    float _Complex *last_l;          
    int have_prev_epoch;     
    uint32_t out_level_n;    
    double   out_level;      
    /* ── lock detector (always on): offset-tap CFAR noise ref + N-look test  */
    float _Complex acc_o;     
    double off_chips;        
    /* The taps' offsets as PHASE WORDS (fractions of one period in 32 bits,
       the code NCO's unit), DERIVED from sf, spacing and off_chips by
       dll_taps_refresh(), from draw_offset() (which init, create and reset
       reach by seed(), after configure_geometry() sets sf and spacing) and
       from set_state -- so the per-sample taps add words and never divide
       (dll_tap_base(), doppler#2397). */
    uint32_t tap_grid;       
    uint32_t tap_spacing;    
    uint32_t tap_off;        
    double noise_guard;      
    uint32_t rng;            
    double noise_ema;        
    double lock_alpha;       
    double lock_sum;         
    size_t lock_count;       
    size_t n_looks;          
    double lock_stat;        
    size_t lock_nz;          
    dp_lockdet_state_t lock;    
    /* ── symbol-timing aid (segments>1 only; period 0 = off).
     *    The same max-power search the per-epoch look-back runs, lifted to
     *    the SYMBOL scale: with the data-symbol period known in partials,
     *    `aid_nhyp = ceil(period)` boundary-phase hypotheses each own a
     *    coherent window of `aid_len` partials per symbol; the hypothesis
     *    whose windows carry the most power (an EMA over symbols) is the
     *    symbol timing, and ITS windows are both the lock detector's looks
     *    and the code discriminator's windows -- coherent over most of a
     *    symbol instead of a quarter-epoch partial, and the loop steers
     *    once per symbol on them (the filter re-timed to that interval,
     *    `inv_upd`). See dp_dll_set_symbol_period(). Rings are heap-owned,
     *    sized at set time, packed field-wise in the state blob like the
     *    chunk buffers. */
    double  sym_period;      
    size_t  aid_len;         
    size_t  aid_nhyp;        
    size_t  aid_ring;        
    size_t  aid_best;        
    uint64_t aid_count;      
    uint64_t aid_last_end;   
    double  aid_alpha;       
    float _Complex *aid_ring_p; 
    float _Complex *aid_ring_o; 
    float _Complex *aid_ring_e; 
    float _Complex *aid_ring_l; 
    double        *aid_power;  
    int owns_code;           
    dll_tlm_t tlm;           
} dp_dll_state_t;

JM_FORCEINLINE float
dll_chip_sign(uint8_t c)
{
    return (c & 1u) ? -1.0f : 1.0f;
}

JM_FORCEINLINE float
dll_replica_word(const dp_dll_state_t *s, uint32_t w)
{
    uint32_t     frac;
    const size_t i  = nco_word_to_grid(w, (uint32_t)(2u * s->sf), &frac);
    size_t       c1 = (i + 1) >> 1; /* the next point's chip */
    if (c1 == s->sf)
        c1 = 0;
    const float mu = nco_frac_to_weight(frac);
    const float v0 = dll_chip_sign(s->code[i >> 1]);
    const float v1 = dll_chip_sign(s->code[c1]);
    return v0 + mu * (v1 - v0);
}

/* Cycles -> u32 phase delta: nco_norm_freq_to_inc() (native/inc/doppler/nco/nco_core.h)
 * is the ONE shared primitive for this conversion -- do not grow a
 * private copy here (a prior copy of this exact formula existed under
 * the name dll_cycles_to_phase_delta() and has been consolidated away;
 * see nco_norm_freq_to_inc()'s own doc comment for why duplicates of this
 * conversion keep drifting). */

void dp_dll_init(dp_dll_state_t *s, const uint8_t *code, size_t code_len, size_t sps,
              double init_chip, double bn, double zeta, double spacing);

JM_FORCEINLINE uint32_t
dll_tap_base(const dp_dll_state_t *s)
{
    return nco_dwell_center(&s->code_nco) - s->tap_grid;
}

JM_FORCEINLINE JM_HOT int
dll_accumulate(dp_dll_state_t *s, float _Complex d)
{
    const uint32_t p = dll_tap_base(s);
    s->acc_p += d * dll_replica_word(s, p);
    s->acc_e += d * dll_replica_word(s, p + s->tap_spacing);
    s->acc_l += d * dll_replica_word(s, p - s->tap_spacing);
    /* nco_step_u32_ovf() (native/inc/doppler/nco/nco_core.h) is the ONE shared
       primitive for "advance one sample, report whether it wrapped" --
       this used to be a private inline reimplementation of exactly that. */
    double sfd = (double)s->sf;
    uint8_t carry;
    (void)nco_step_u32_ovf(&s->code_nco, &carry);
    s->chip_pos = ((double)s->code_nco.phase / 4294967296.0) * sfd;
    return carry;
}

JM_FORCEINLINE JM_HOT void
dll_lock_accumulate(dp_dll_state_t *s, float _Complex d)
{
    s->acc_o += d * dll_replica_word(s, dll_tap_base(s) + s->tap_off);
}

void dp_dll_lock_look(dp_dll_state_t *s, double norm);

void dp_dll_lock_epoch(dp_dll_state_t *s);

JM_FORCEINLINE JM_HOT void
dll_steer(dp_dll_state_t *s, double ep, double lp, double pp)
{
    double e = 0.5 * (ep - lp) / (pp + DLL_EPS);
    if (e > DLL_DISC_CLAMP)
        e = DLL_DISC_CLAMP;
    else if (e < -DLL_DISC_CLAMP)
        e = -DLL_DISC_CLAMP;
    s->last_error = e;
    s->err_sum += e;
    s->err_n++;
    if (s->coast)
        return; /* held: the discriminator read (last_error, the probe, the
                   sum) but not filtered, phase_inc as it stands
                   (dp_dll_set_coast) -- a holder coasting on another clock
                   reads where the signal sits against the held phase, which
                   is what it corrects on */
    (void)dp_loop_filter_step(&s->lf, e);
    double integ = s->lf.integ;
    double pe    = s->lf.kp * e;
    double u     = s->inv_upd;
    double ctrl  = u * (s->ctrl_i * integ + s->ctrl_p * pe);
    s->code_rate = 1.0 + u * (s->rate_i * integ + s->rate_p * pe);
    s->code_nco.phase_inc
        = nco_norm_freq_to_inc(s->inv_tsamps * (1.0 + s->rate_aid) + ctrl);
}

JM_FORCEINLINE JM_HOT void
dll_update(dp_dll_state_t *s)
{
    float me = cabsf(s->acc_e), ml = cabsf(s->acc_l), mp = cabsf(s->acc_p);
    dll_steer(s, (double)me * me, (double)ml * ml, (double)mp * mp);
}

dp_dll_state_t *dp_dll_create(const uint8_t *code, size_t code_len, size_t sps, double init_chip, double bn, double zeta, double spacing, size_t segments);

int dp_dll_params_ok(size_t code_len, double init_chip, double bn, double zeta, double spacing, size_t segments);

#define DLL_AID_MAX_PERIOD 1048576.0

int dp_dll_symbol_period_ok(size_t segments, double partials_per_symbol);

size_t dp_dll_lookback_segments(size_t tsamps, double max_error_db);

void dp_dll_destroy(dp_dll_state_t *state);

void dp_dll_reset(dp_dll_state_t *state);

size_t dp_dll_steps_max_out(dp_dll_state_t *state);

size_t dp_dll_steps(dp_dll_state_t *state, const float _Complex *x, size_t x_len, float _Complex *out, size_t max_out);

int dp_dll_configure(dp_dll_state_t *state, double bn, double zeta);
double dp_dll_get_bn(const dp_dll_state_t *state);
int dp_dll_set_bn(dp_dll_state_t *state, double val);

void dp_dll_set_rate_aid(dp_dll_state_t *state, double rate_aid);

void dp_dll_set_coast(dp_dll_state_t *state, int coast);

void dp_dll_hold_here(dp_dll_state_t *state);

int dp_dll_set_symbol_period(dp_dll_state_t *state, double partials_per_symbol);

size_t dp_dll_get_symbol_window(const dp_dll_state_t *state);

int dp_dll_set_lock_verify(dp_dll_state_t *state, uint32_t n_up, uint32_t n_down);
double dp_dll_get_code_phase(const dp_dll_state_t *state);

void dp_dll_set_code_phase(dp_dll_state_t *state, double chips);
double dp_dll_get_code_rate(const dp_dll_state_t *state);

size_t dp_dll_take_error(dp_dll_state_t *state, double *sum);

double dp_dll_take_error_mean(dp_dll_state_t *state);
double dp_dll_get_last_error(const dp_dll_state_t *state);
size_t dp_dll_get_segments(const dp_dll_state_t *state);

int dp_dll_configure_lock(dp_dll_state_t *state, double pfa, size_t n_looks, double ref_snr_db);

void dp_dll_configure_lock_raw(dp_dll_state_t *state, double up_thresh,
                            double down_thresh, size_t n_looks, double alpha,
                            uint32_t n_up, uint32_t n_down);

int dp_dll_get_locked(const dp_dll_state_t *state);

double dp_dll_get_lock_stat(const dp_dll_state_t *state);

double dp_dll_get_noise_est(const dp_dll_state_t *state);

void dp_dll_tlm_flush(const dp_dll_state_t *s);

int dp_dll_set_telemetry(dp_dll_state_t *state, dp_tlm_t * tlm, const char * prefix, uint32_t decim);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition+field-wise: loop_filter child (POD-embedded) + embedded NCO
 * (POD) + running correlators/loop/lock state; borrowed `code` pointer
 * restored by create; the segments>1 chunk/lookback buffers (heap-owned,
 * pointers, NOT part of the whole-struct snapshot) are packed/restored
 * field-wise when segments > 1. */
#define DLL_STATE_MAGIC DP_FOURCC ('D','L','L',' ')
#define DLL_STATE_VERSION 15u /* v15: the output level, out_level/out_level_n (#2495); v14: the taps' phase words (#2397); v13: wrap_pending/wrap_await (#1287); v12: the gain table and err_sum/err_n (#1280); v11: coast (#1271); v10: aid_last_end (#1264); v9: the aid's early/late rings + inv_upd
                                (the loop steers once per symbol on the
                                aided window).
                                v8: symbol-period aid fields + rings;
                                v7: `rate_aid` carrier-aiding field added
                                (whole-struct snapshot, so the blob grew).
                                v6: `sums` scratch field added to the
                                struct (pure epoch-local scratch, not
                                serialized -- grows sizeof(dp_dll_state_t)
                                regardless, so the version marks the
                                layout change) (v5: precomputed
                                inv_tsamps/inv_tsamps2/inv_tsamps_sf
                                fields added to the struct; v4: fixed-
                                point code_nco + segments>1 chunked
                                lookback buffers; see dll_core.c) */
size_t dp_dll_state_bytes (const dp_dll_state_t *state);
void dp_dll_get_state (const dp_dll_state_t *state, void *blob);
int dp_dll_set_state (dp_dll_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DLL_CORE_H */
```


