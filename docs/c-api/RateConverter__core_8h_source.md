

# File RateConverter\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**RateConverter**](dir_f243cfbf2f82d96e6953dd99cc2498dd.md) **>** [**RateConverter\_core.h**](RateConverter__core_8h.md)

[Go to the documentation of this file](RateConverter__core_8h.md)


```C++

#ifndef RATE_CONVERTER_CORE_H
#define RATE_CONVERTER_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"

#include "doppler/dp_complex.h"
#include <stdbool.h>
#include <stddef.h>
#include "doppler/resamp/resamp_core.h"
#include "doppler/fir/fir_core.h"
#include "doppler/agc/agc_core.h"
#include "doppler/dp_tlm/dp_tlm_core.h"

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
     bank, i.e. everything dp_RateConverter_create() builds).  Kept so
     dp_RateConverter_set_rate() can re-plan without losing the pulse. */
  int    pulse;      
  double beta;       
  size_t span;       
  double pulse_sps;  
  size_t num_phases; 
  bool narrow_pulse;
  /* ── Pre-terminal AGC (NULL = off, which is the default and what every
     constructor builds).  See RateConverter_enable_agc(). ─────────────── */
  dp_agc_state_t *agc;          
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
} dp_RateConverter_state_t;

dp_RateConverter_state_t *dp_RateConverter_create (double rate, int compensate);

dp_RateConverter_state_t *
RateConverter_create_matched (double rate, int compensate, int pulse,
                              double beta, size_t span, double pulse_sps,
                              size_t num_phases);

bool dp_RateConverter_get_clipped (const dp_RateConverter_state_t *s);

bool dp_RateConverter_get_narrow_pulse (const dp_RateConverter_state_t *s);

size_t RateConverter_num_stages (const dp_RateConverter_state_t *s);

double RateConverter_gain (const dp_RateConverter_state_t *s);
const char *dp_RateConverter_stages_value (const dp_RateConverter_state_t *s,
                                        size_t i);

size_t RateConverter_num_bank_shape (const dp_RateConverter_state_t *s);
size_t dp_RateConverter_bank_shape_value (const dp_RateConverter_state_t *s,
                                       size_t i);

int RateConverter_enable_agc (dp_RateConverter_state_t *s, double bn_sym,
                              double alpha);

double RateConverter_agc_ref_db (const dp_RateConverter_state_t *s);

double RateConverter_agc_gain_db (const dp_RateConverter_state_t *s);

int RateConverter_set_telemetry (dp_RateConverter_state_t *s, dp_tlm_t *tlm,
                                 const char *prefix, uint32_t decim);

void dp_RateConverter_destroy (dp_RateConverter_state_t *s);

void dp_RateConverter_reset (dp_RateConverter_state_t *s);

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

size_t dp_RateConverter_state_bytes (const dp_RateConverter_state_t *s);
void dp_RateConverter_get_state (const dp_RateConverter_state_t *s, void *blob);
int dp_RateConverter_set_state (dp_RateConverter_state_t *s, const void *blob);

size_t dp_RateConverter_execute (dp_RateConverter_state_t *s,
                              const float _Complex *in, size_t n_in,
                              float _Complex *out, size_t max_out);

size_t dp_RateConverter_execute_max_out (dp_RateConverter_state_t *s);

size_t dp_RateConverter_execute_ctrl_max_out (dp_RateConverter_state_t *s);
size_t dp_RateConverter_execute_ctrl_push_max_out (dp_RateConverter_state_t *s);

size_t dp_RateConverter_execute_ctrl (dp_RateConverter_state_t *s,
                                   const float _Complex *x, size_t n_in,
                                   double ctrl, float _Complex *out,
                                   size_t max_out);

size_t dp_RateConverter_execute_ctrl_push (dp_RateConverter_state_t *s,
                                        float _Complex x, double ctrl,
                                        float _Complex *out, size_t max_out);

size_t RateConverter_execute_ctrl_push_tap (dp_RateConverter_state_t *s,
                                            float _Complex x, double ctrl,
                                            float _Complex *out,
                                            size_t max_out,
                                            float _Complex *pre_out,
                                            int *n_pre);

double RateConverter_get_bank_sps (const dp_RateConverter_state_t *s);

double dp_RateConverter_get_rate (const dp_RateConverter_state_t *s);

void dp_RateConverter_set_rate (dp_RateConverter_state_t *s, double rate);

int RateConverter_stage_label (dp_RateConverter_state_t *s, int i,
                               char *buf, size_t len);

size_t RateConverter_convert (double rate, int compensate,
                              const float _Complex *in, size_t n_in,
                              float _Complex *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* RATE_CONVERTER_CORE_H */
```


