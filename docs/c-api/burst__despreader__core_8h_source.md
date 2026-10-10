

# File burst\_despreader\_core.h

[**File List**](files.md) **>** [**burst\_despreader**](dir_28ffcd911995597d422ebd972d69802b.md) **>** [**burst\_despreader\_core.h**](burst__despreader__core_8h.md)

[Go to the documentation of this file](burst__despreader__core_8h.md)


```C++

#ifndef DP_BURST_DESPREADER_CORE_H
#define DP_BURST_DESPREADER_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include "doppler/loop_filter/loop_filter_core.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  /* ── configuration (immutable after create) ── */
  uint8_t *code;   
  size_t   sf;     
  size_t   sps;    
  size_t   tsamps; 
  double   seed_w; 
  double   seed_chip; 
  /* ── optional acquisition preamble (distinct acq code) ── */
  uint8_t *acq_code; 
  size_t   acq_sf;   
  size_t   acq_reps; 
  size_t   preamble_left; 
  /* ── tracking loops (embedded by value, shared engine) ── */
  dp_loop_filter_state_t lf_car;  
  dp_loop_filter_state_t lf_code; 
  /* ── carrier NCO (inline, radians) ── */
  double car_phase; 
  double car_w;     
  /* ── code phase / integrate-and-dump ── */
  double        chip_pos;  
  double        code_rate; 
  float _Complex acc_e;     
  float _Complex acc_p;     
  float _Complex acc_l;     
  /* ── status read-backs: cumulative over the burst (reset re-arms) ── */
  double lock_metric; 
  double snr_est;     
  double sum_lock;    
  double sum_re2;     
  double sum_im2;     
  size_t stat_n;      
} dp_burst_despreader_state_t;

dp_burst_despreader_state_t *dp_burst_despreader_create(const uint8_t *code, size_t code_len, size_t sf, size_t sps, double init_norm_freq, double init_chip_phase, double bn_carrier, double bn_code);

void dp_burst_despreader_set_acq(dp_burst_despreader_state_t *state, const uint8_t *acq_code,
                        size_t acq_code_len, size_t acq_reps);

void dp_burst_despreader_destroy(dp_burst_despreader_state_t *state);

void dp_burst_despreader_reset(dp_burst_despreader_state_t *state);









size_t dp_burst_despreader_steps_max_out (dp_burst_despreader_state_t *state);

size_t dp_burst_despreader_steps (dp_burst_despreader_state_t *state, const float _Complex *x,
                         size_t x_len, float _Complex *out, size_t max_out);

size_t dp_burst_despreader_bits_max_out (dp_burst_despreader_state_t *state);

size_t dp_burst_despreader_bits (dp_burst_despreader_state_t *state, const float _Complex *x,
                        size_t x_len, uint8_t *out, size_t max_out);

double dp_burst_despreader_get_bn_carrier (const dp_burst_despreader_state_t *state);
void dp_burst_despreader_set_bn_carrier (dp_burst_despreader_state_t *state, double val);
double dp_burst_despreader_get_bn_code (const dp_burst_despreader_state_t *state);
void dp_burst_despreader_set_bn_code (dp_burst_despreader_state_t *state, double val);
double dp_burst_despreader_get_norm_freq (const dp_burst_despreader_state_t *state);
void dp_burst_despreader_set_norm_freq (dp_burst_despreader_state_t *state, double val);
double dp_burst_despreader_get_code_phase (const dp_burst_despreader_state_t *state);
double dp_burst_despreader_get_lock_metric (const dp_burst_despreader_state_t *state);
double dp_burst_despreader_get_snr_est (const dp_burst_despreader_state_t *state);

double dp_burst_despreader_get_lock_stat (const dp_burst_despreader_state_t *state);

size_t dp_burst_despreader_get_stat_n (const dp_burst_despreader_state_t *state);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * The struct (loop_filter children POD-embedded) and then the acq code. The
 * data code is create-time config, rebuilt by create(); the acq code is a
 * setter's value -- only set_acq() sets it -- so it travels (#2022), and its
 * length is part of the blob's size. The loops' bn travels too: it is
 * set_bn_carrier()'s and set_bn_code()'s value. set_state refuses, writing
 * nothing, a blob from another sf, sps, seed or acq-code length; one whose
 * preamble fields no set_acq() call could leave; one whose loop gains are
 * not the ones its bn derives; and one holding a non-finite number, as an
 * object fed a NaN sample does (#2041). */
#define BURST_DESPREADER_STATE_MAGIC DP_FOURCC ('B','D','S','P')
#define BURST_DESPREADER_STATE_VERSION                                       \
  3u /* v3: the acq code travels; sizing keys checked first (#2041) */
size_t dp_burst_despreader_state_bytes (const dp_burst_despreader_state_t *state);
void dp_burst_despreader_get_state (const dp_burst_despreader_state_t *state, void *blob);
int dp_burst_despreader_set_state (dp_burst_despreader_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* BURST_DESPREADER_CORE_H */
```


