

# File burst\_despreader\_core.h

[**File List**](files.md) **>** [**burst\_despreader**](dir_311cad0a77759dd1ff95e00f622e2f49.md) **>** [**burst\_despreader\_core.h**](burst__despreader__core_8h.md)

[Go to the documentation of this file](burst__despreader__core_8h.md)


```C++

#ifndef BURST_DESPREADER_CORE_H
#define BURST_DESPREADER_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "loop_filter/loop_filter_core.h"
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
  loop_filter_state_t lf_car;  
  loop_filter_state_t lf_code; 

  /* ── carrier NCO (inline, radians) ── */
  double car_phase; 
  double car_w;     

  /* ── code phase / integrate-and-dump ── */
  double        chip_pos;  
  double        code_rate; 
  float complex acc_e;     
  float complex acc_p;     
  float complex acc_l;     

  /* ── status read-backs: cumulative over the burst (reset re-arms) ── */
  double lock_metric; 
  double snr_est;     
  double sum_lock;    
  double sum_re2;     
  double sum_im2;     
  size_t stat_n;      
} burst_despreader_state_t;

burst_despreader_state_t *burst_despreader_create(const uint8_t *code, size_t code_len, size_t sf, size_t sps, double init_norm_freq, double init_chip_phase, double bn_carrier, double bn_code);

void burst_despreader_set_acq(burst_despreader_state_t *state, const uint8_t *acq_code,
                        size_t acq_code_len, size_t acq_reps);

void burst_despreader_destroy(burst_despreader_state_t *state);

void burst_despreader_reset(burst_despreader_state_t *state);









size_t burst_despreader_steps_max_out (burst_despreader_state_t *state);

size_t burst_despreader_steps (burst_despreader_state_t *state, const float complex *x,
                         size_t x_len, float complex *out, size_t max_out);

size_t burst_despreader_bits_max_out (burst_despreader_state_t *state);

size_t burst_despreader_bits (burst_despreader_state_t *state, const float complex *x,
                        size_t x_len, uint8_t *out, size_t max_out);

double burst_despreader_get_bn_carrier (const burst_despreader_state_t *state);
void burst_despreader_set_bn_carrier (burst_despreader_state_t *state, double val);
double burst_despreader_get_bn_code (const burst_despreader_state_t *state);
void burst_despreader_set_bn_code (burst_despreader_state_t *state, double val);
double burst_despreader_get_norm_freq (const burst_despreader_state_t *state);
void burst_despreader_set_norm_freq (burst_despreader_state_t *state, double val);
double burst_despreader_get_code_phase (const burst_despreader_state_t *state);
double burst_despreader_get_lock_metric (const burst_despreader_state_t *state);
double burst_despreader_get_snr_est (const burst_despreader_state_t *state);

double burst_despreader_get_lock_stat (const burst_despreader_state_t *state);

size_t burst_despreader_get_stat_n (const burst_despreader_state_t *state);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * whole-struct snapshot (loop_filter children POD-embedded);
 * the owned code + acq_code pointers are config, restored by create. */
#define BURST_DESPREADER_STATE_MAGIC DP_FOURCC ('B','D','S','P')
#define BURST_DESPREADER_STATE_VERSION 2u /* v2: cumulative burst statistics */
size_t burst_despreader_state_bytes (const burst_despreader_state_t *state);
void burst_despreader_get_state (const burst_despreader_state_t *state, void *blob);
int burst_despreader_set_state (burst_despreader_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* BURST_DESPREADER_CORE_H */
```


