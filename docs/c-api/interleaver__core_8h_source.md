

# File interleaver\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**interleaver**](dir_46ba54d679b7d3fa44b8264f360065a9.md) **>** [**interleaver\_core.h**](interleaver__core_8h.md)

[Go to the documentation of this file](interleaver__core_8h.md)


```C++

#ifndef INTERLEAVER_CORE_H
#define INTERLEAVER_CORE_H

#include "dp_interleave.h"

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
  } interleaver_state_t;

  interleaver_state_t *interleaver_create (size_t rows, size_t cols,
                                           size_t unit_bits);

  interleaver_state_t *interleaver_create_rx (size_t rows, size_t cols,
                                              size_t unit_bits);

  void interleaver_destroy (interleaver_state_t *state);

  void interleaver_reset (interleaver_state_t *state);

  size_t interleaver_get_block_bits (const interleaver_state_t *state);

  size_t interleaver_interleave_max_out (const interleaver_state_t *state,
                                         size_t n_in);

  size_t interleaver_deinterleave_max_out (const interleaver_state_t *state,
                                           size_t n_in);

  size_t
  interleaver_deinterleave_soft_max_out (const interleaver_state_t *state,
                                         size_t n_in);

  size_t interleaver_interleave (interleaver_state_t *state,
                                 const uint8_t *in, size_t n_in, uint8_t *out,
                                 size_t max_out);

  size_t interleaver_deinterleave (interleaver_state_t *state,
                                   const uint8_t *in, size_t n_in,
                                   uint8_t *out, size_t max_out);

  size_t interleaver_deinterleave_soft (interleaver_state_t *state,
                                        const float *in, size_t n_in,
                                        float *out, size_t max_out);

  size_t interleaver_get_burst_len (const interleaver_state_t *state);

  size_t interleaver_get_separation (const interleaver_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* INTERLEAVER_CORE_H */
```


