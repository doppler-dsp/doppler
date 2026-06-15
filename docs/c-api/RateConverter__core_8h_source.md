

# File RateConverter\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**RateConverter**](dir_ab9e07a54a3e9554c466f24859c37292.md) **>** [**RateConverter\_core.h**](RateConverter__core_8h.md)

[Go to the documentation of this file](RateConverter__core_8h.md)


```C++

#ifndef RATE_CONVERTER_CORE_H
#define RATE_CONVERTER_CORE_H

#include "clib_common.h"

#include <complex.h>
#include <stddef.h>
#include "resamp/resamp_core.h"
#include "fir/fir_core.h"

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

typedef struct
{
  double         rate;                        
  int            compensate;                  
  int            n_stages;                    
  rc_stage_t     stage_types[RC_MAX_STAGES];  
  void          *stage_ptrs[RC_MAX_STAGES];   
  float _Complex *bufs[2];
  size_t          buf_cap;
} RateConverter_state_t;

RateConverter_state_t *RateConverter_create (double rate, int compensate);

void RateConverter_destroy (RateConverter_state_t *s);

void RateConverter_reset (RateConverter_state_t *s);

size_t RateConverter_execute (RateConverter_state_t *s,
                              const float _Complex *in, size_t n_in,
                              float _Complex *out, size_t max_out);

size_t RateConverter_execute_max_out (RateConverter_state_t *s);

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


