

# File RateConverter\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**RateConverter**](dir_ab9e07a54a3e9554c466f24859c37292.md) **>** [**RateConverter\_core.h**](RateConverter__core_8h.md)

[Go to the documentation of this file](RateConverter__core_8h.md)


```C++

#ifndef RATE_CONVERTER_CORE_H
#define RATE_CONVERTER_CORE_H

#include "clib_common.h"
#include "dp_state.h"

#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include "resamp/resamp_core.h"
#include "fir/fir_core.h"
#include "agc/agc_core.h"
#include "dp_tlm/dp_tlm_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RC_MAX_STAGES 3

typedef enum
{
  RC_STAGE_HB     = 0, 
  RC_STAGE_CIC    = 1, 
  RC_STAGE_RESAMP = 2, 
} rc_stage_t;

typedef enum
{
  RC_PULSE_IANDD = 0, 
  RC_PULSE_RRC   = 1, 
  RC_PULSE_NONE  = 2, 
} rc_pulse_t;

typedef struct
{
  double         rate;                        
  int            compensate;                  
  int            n_stages;                    
  rc_stage_t     stage_types[RC_MAX_STAGES];  
  void          *stage_ptrs[RC_MAX_STAGES];   
  float _Complex *bufs[2];
  size_t          buf_cap;
  /* Matched-filter configuration (RC_PULSE_NONE = plain Kaiser terminal
     bank, i.e. everything RateConverter_create() builds).  Kept so
     RateConverter_set_rate() can re-plan without losing the pulse. */
  int    pulse;      
  double beta;       
  size_t span;       
  double pulse_sps;  
  size_t num_phases; 
  bool narrow_pulse;
  /* ── Pre-terminal AGC (NULL = off, which is the default and what every
     constructor builds).  See RateConverter_enable_agc(). ─────────────── */
  agc_state_t *agc;          
  double       bank_sps;     
  double       bank_e0;      
  double       agc_ref_db;   
  double       agc_bn_sym;   
  double       agc_alpha;    
  struct
  {
    dp_tlm_t *ctx;                   
    char      prefix[DP_TLM_NAME_MAX]; 
    uint32_t  decim;                 
  } agc_tlm_req;
} RateConverter_state_t;

RateConverter_state_t *RateConverter_create (double rate, int compensate);

RateConverter_state_t *
RateConverter_create_matched (double rate, int compensate, int pulse,
                              double beta, size_t span, double pulse_sps,
                              size_t num_phases);

bool RateConverter_get_clipped (const RateConverter_state_t *s);

bool RateConverter_get_narrow_pulse (const RateConverter_state_t *s);

size_t RateConverter_num_stages (const RateConverter_state_t *s);

double RateConverter_gain (const RateConverter_state_t *s);
const char *RateConverter_stages_value (const RateConverter_state_t *s,
                                        size_t i);

size_t RateConverter_num_bank_shape (const RateConverter_state_t *s);
size_t RateConverter_bank_shape_value (const RateConverter_state_t *s,
                                       size_t i);

int RateConverter_enable_agc (RateConverter_state_t *s, double bn_sym,
                              double alpha);

double RateConverter_agc_ref_db (const RateConverter_state_t *s);

double RateConverter_agc_gain_db (const RateConverter_state_t *s);

int RateConverter_set_telemetry (RateConverter_state_t *s, dp_tlm_t *tlm,
                                 const char *prefix, uint32_t decim);

void RateConverter_destroy (RateConverter_state_t *s);

void RateConverter_reset (RateConverter_state_t *s);

/* Serializable state (standard bytes interface; see dp_state.h): the standard
 * envelope followed by the concatenated mutable state of the active cascade
 * stages (HB / CIC[+comp FIR] / Resampler), in cascade order — each a
 * self-contained sub-blob with its own leaf envelope.  The stage plan is config
 * (rebuilt from rate), so a same-rate RateConverter round-trips exactly.
 * v2: an enabled pre-terminal AGC appends its seed scalars and its own
 * sub-blob after the stages. A converter with the AGC off writes exactly the
 * bytes v1 did — but the version still moves, because nothing in the blob
 * distinguishes an AGC-off v2 from a v1, and the size check alone cannot. */
#define RC_STATE_MAGIC DP_FOURCC ('R', 'C', 'V', 'T')
#define RC_STATE_VERSION 2u

size_t RateConverter_state_bytes (const RateConverter_state_t *s);
void RateConverter_get_state (const RateConverter_state_t *s, void *blob);
int RateConverter_set_state (RateConverter_state_t *s, const void *blob);

size_t RateConverter_execute (RateConverter_state_t *s,
                              const float _Complex *in, size_t n_in,
                              float _Complex *out, size_t max_out);

size_t RateConverter_execute_max_out (RateConverter_state_t *s);

size_t RateConverter_execute_ctrl_max_out (RateConverter_state_t *s);
size_t RateConverter_execute_ctrl_push_max_out (RateConverter_state_t *s);

size_t RateConverter_execute_ctrl (RateConverter_state_t *s,
                                   const float _Complex *x, size_t n_in,
                                   double ctrl, float _Complex *out,
                                   size_t max_out);

size_t RateConverter_execute_ctrl_push (RateConverter_state_t *s,
                                        float _Complex x, double ctrl,
                                        float _Complex *out, size_t max_out);

size_t RateConverter_execute_ctrl_push_tap (RateConverter_state_t *s,
                                            float _Complex x, double ctrl,
                                            float _Complex *out,
                                            size_t max_out,
                                            float _Complex *pre_out,
                                            int *n_pre);

double RateConverter_get_bank_sps (const RateConverter_state_t *s);

double RateConverter_get_rate (const RateConverter_state_t *s);

void RateConverter_set_rate (RateConverter_state_t *s, double rate);

int RateConverter_stage_label (RateConverter_state_t *s, int i,
                               char *buf, size_t len);

size_t RateConverter_convert (double rate, int compensate,
                              const float _Complex *in, size_t n_in,
                              float _Complex *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* RATE_CONVERTER_CORE_H */
```


