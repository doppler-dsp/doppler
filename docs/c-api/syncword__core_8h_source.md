

# File syncword\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**syncword**](dir_8170b734982c9e3c4a0c2955e2cfa64d.md) **>** [**syncword\_core.h**](syncword__core_8h.md)

[Go to the documentation of this file](syncword__core_8h.md)


```C++

#ifndef SYNCWORD_CORE_H
#define SYNCWORD_CORE_H

#include "clib_common.h"
#include "dp_syncword.h"
#include "jm_perf.h"
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
} syncword_state_t;

syncword_state_t *syncword_create (const uint8_t *marker, size_t marker_len);

void syncword_destroy (syncword_state_t *state);

syncword_hit_t syncword_find (syncword_state_t *state, const uint8_t *bits,
                              size_t bits_len, uint32_t max_errors);

double syncword_pfa (syncword_state_t *state, uint32_t max_errors);

int syncword_max_errors_for (syncword_state_t *state, size_t window_bits,
                             double pfa);
#ifdef __cplusplus
}
#endif

#endif /* SYNCWORD_CORE_H */
```


