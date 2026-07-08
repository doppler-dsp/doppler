

# File despreader\_core.h

[**File List**](files.md) **>** [**despreader**](dir_9949992fff5aebed427f83f9eaa478ca.md) **>** [**despreader\_core.h**](despreader__core_8h.md)

[Go to the documentation of this file](despreader__core_8h.md)


```C++

#ifndef DESPREADER_CORE_H
#define DESPREADER_CORE_H

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

  /* ── status read-backs ── */
  double lock_metric; 
  double snr_est;     
} despreader_state_t;

despreader_state_t *despreader_create(const uint8_t *code, size_t code_len, size_t sf, size_t sps, double init_norm_freq, double init_chip_phase, double bn_carrier, double bn_code);

void despreader_set_acq(despreader_state_t *state, const uint8_t *acq_code,
                        size_t acq_code_len, size_t acq_reps);

void despreader_destroy(despreader_state_t *state);

void despreader_reset(despreader_state_t *state);









size_t despreader_steps_max_out (despreader_state_t *state);

size_t despreader_steps (despreader_state_t *state, const float complex *x,
                         size_t x_len, float complex *out, size_t max_out);

size_t despreader_bits_max_out (despreader_state_t *state);

size_t despreader_bits (despreader_state_t *state, const float complex *x,
                        size_t x_len, uint8_t *out, size_t max_out);

double despreader_get_bn_carrier (const despreader_state_t *state);
void despreader_set_bn_carrier (despreader_state_t *state, double val);
double despreader_get_bn_code (const despreader_state_t *state);
void despreader_set_bn_code (despreader_state_t *state, double val);
double despreader_get_norm_freq (const despreader_state_t *state);
void despreader_set_norm_freq (despreader_state_t *state, double val);
double despreader_get_code_phase (const despreader_state_t *state);
double despreader_get_lock_metric (const despreader_state_t *state);
double despreader_get_snr_est (const despreader_state_t *state);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * whole-struct snapshot (loop_filter children POD-embedded);
 * the owned code + acq_code pointers are config, restored by create. */
#define DESPREADER_STATE_MAGIC DP_FOURCC ('D','S','P','R')
#define DESPREADER_STATE_VERSION 1u
size_t despreader_state_bytes (const despreader_state_t *state);
void despreader_get_state (const despreader_state_t *state, void *blob);
int despreader_set_state (despreader_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DESPREADER_CORE_H */
```


