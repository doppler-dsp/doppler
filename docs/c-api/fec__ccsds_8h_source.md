

# File fec\_ccsds.h

[**File List**](files.md) **>** [**fec**](dir_df2a893a07d8c9ef377268dabdb4859f.md) **>** [**fec\_ccsds.h**](fec__ccsds_8h.md)

[Go to the documentation of this file](fec__ccsds_8h.md)


```C++

#ifndef FEC_CCSDS_H
#define FEC_CCSDS_H

#include "conv/conv_core.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define FEC_CCSDS_ASM 0x1ACFFC1DuL

#define FEC_CCSDS_ASM_BITS 32

#define FEC_CCSDS_RAND_PERIOD 255

#define FEC_CONV_K 7

  void fec_ccsds_asm_bits (uint8_t *out);

  typedef struct
  {
    size_t   offset;   
    int      inverted; 
    unsigned errors;   
  } fec_asm_hit_t;

  int fec_ccsds_asm_find (const uint8_t *bits, size_t n_bits,
                          unsigned max_errors, fec_asm_hit_t *hit);

  void fec_ccsds_randomise (uint8_t *bits, size_t n);

  extern const conv_code_t FEC_CCSDS_CONV;

  static inline size_t
  fec_conv_max_out (size_t n)
  {
    return 2u * n;
  }

  void fec_ccsds_rand_seq (uint8_t *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* FEC_CCSDS_H */
```


