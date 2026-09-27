

# File syncword\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**syncword**](dir_e299403a0e03806f7aa46ec15fc4c91a.md) **>** [**syncword\_core.h**](syncword__core_8h.md)

[Go to the documentation of this file](syncword__core_8h.md)


```C++

#ifndef DP_SYNCWORD_CORE_H
#define DP_SYNCWORD_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_syncword.h"
#include "doppler/jm_perf.h"
#include "doppler/detection/detection_core.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  int      found;    
  size_t   offset;   
  int      inverted; 
  uint32_t errors;   
} syncword_hit_t;

typedef struct
{
  uint8_t *marker; 
  /*<<property_struct_fields>>*/
  size_t nbits;
} dp_syncword_state_t;

dp_syncword_state_t *dp_syncword_create (const uint8_t *marker, size_t marker_len);

void dp_syncword_destroy (dp_syncword_state_t *state);

syncword_hit_t dp_syncword_find (dp_syncword_state_t *state, const uint8_t *bits,
                              size_t bits_len, uint32_t max_errors);

double dp_syncword_pfa (dp_syncword_state_t *state, uint32_t max_errors);

int dp_syncword_max_errors_for (dp_syncword_state_t *state, size_t window_bits,
                             double pfa);
#ifdef __cplusplus
}
#endif

#endif /* SYNCWORD_CORE_H */
```


