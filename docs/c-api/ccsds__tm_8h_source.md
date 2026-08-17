

# File ccsds\_tm.h

[**File List**](files.md) **>** [**ccsds\_tm**](dir_c2a51186254da91e75ac1924b4969fdd.md) **>** [**ccsds\_tm.h**](ccsds__tm_8h.md)

[Go to the documentation of this file](ccsds__tm_8h.md)


```C++

#ifndef CCSDS_TM_H
#define CCSDS_TM_H

#include "conv/conv_core.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define FEC_CCSDS_ASM 0x1ACFFC1DuL

#define CCSDS_TM_ASM_BITS 32

#define CCSDS_TM_RAND_PERIOD 255

#define FEC_CONV_K 7

  void ccsds_tm_asm_bits (uint8_t *out);

  typedef struct
  {
    size_t   offset;   
    int      inverted; 
    unsigned errors;   
  } ccsds_tm_asm_hit_t;

  int ccsds_tm_asm_find (const uint8_t *bits, size_t n_bits,
                          unsigned max_errors, ccsds_tm_asm_hit_t *hit);

  void ccsds_tm_randomise (uint8_t *bits, size_t n);

  extern const conv_code_t CCSDS_TM_CONV;

  static inline size_t
  ccsds_tm_conv_max_out (size_t n)
  {
    return 2u * n;
  }

  void ccsds_tm_rand_seq (uint8_t *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* CCSDS_TM_H */
```


