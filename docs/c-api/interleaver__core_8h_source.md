

# File interleaver\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**interleaver**](dir_bb303c16bb49af033112c76b5cc87569.md) **>** [**interleaver\_core.h**](interleaver__core_8h.md)

[Go to the documentation of this file](interleaver__core_8h.md)


```C++

#ifndef DP_INTERLEAVER_CORE_H
#define DP_INTERLEAVER_CORE_H

#include "doppler/dp_interleave.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    size_t rows;      
    size_t cols;      
    size_t unit_bits; 
  } dp_interleaver_state_t;

  dp_interleaver_state_t *dp_interleaver_create (size_t rows, size_t cols,
                                           size_t unit_bits);

  dp_interleaver_state_t *interleaver_create_rx (size_t rows, size_t cols,
                                              size_t unit_bits);

  void dp_interleaver_destroy (dp_interleaver_state_t *state);

  void dp_interleaver_reset (dp_interleaver_state_t *state);

  size_t dp_interleaver_get_block_bits (const dp_interleaver_state_t *state);

  size_t dp_interleaver_interleave_max_out (const dp_interleaver_state_t *state,
                                         size_t n_in);

  size_t dp_interleaver_deinterleave_max_out (const dp_interleaver_state_t *state,
                                           size_t n_in);

  size_t
  dp_interleaver_deinterleave_soft_max_out (const dp_interleaver_state_t *state,
                                         size_t n_in);

  size_t dp_interleaver_interleave (dp_interleaver_state_t *state,
                                 const uint8_t *in, size_t n_in, uint8_t *out,
                                 size_t max_out);

  size_t dp_interleaver_deinterleave (dp_interleaver_state_t *state,
                                   const uint8_t *in, size_t n_in,
                                   uint8_t *out, size_t max_out);

  size_t dp_interleaver_deinterleave_soft (dp_interleaver_state_t *state,
                                        const float *in, size_t n_in,
                                        float *out, size_t max_out);

  size_t dp_interleaver_get_burst_len (const dp_interleaver_state_t *state);

  size_t dp_interleaver_get_separation (const dp_interleaver_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* INTERLEAVER_CORE_H */
```


