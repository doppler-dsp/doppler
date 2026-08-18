

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

#define CCSDS_TM_ASM 0x1ACFFC1DuL

#define CCSDS_TM_ASM_BITS 32

  typedef struct
  {
    uint32_t taps;   
    uint32_t seed;   
    unsigned stages; 
    size_t   period; 
  } ccsds_tm_rand_t;

  extern const ccsds_tm_rand_t CCSDS_TM_RAND;

  extern const ccsds_tm_rand_t CCSDS_TM_RAND_LEGACY;

#define CCSDS_TM_RAND_PERIOD 131071

  typedef struct
  {
    uint32_t reg;
    uint32_t taps;
    unsigned stages;
  } ccsds_tm_rand_state_t;

  void ccsds_tm_rand_init (ccsds_tm_rand_state_t *s,
                           const ccsds_tm_rand_t *r);

  uint8_t ccsds_tm_rand_step (ccsds_tm_rand_state_t *s);

#define CCSDS_TM_CONV_K 7

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

  void ccsds_tm_randomise_with (const ccsds_tm_rand_t *r, uint8_t *bits,
                                size_t n);

  extern const conv_code_t CCSDS_TM_CONV;

  static inline size_t
  ccsds_tm_conv_max_out (size_t n)
  {
    return 2u * n;
  }

  void ccsds_tm_rand_seq (uint8_t *out, size_t n);

  void ccsds_tm_rand_seq_with (const ccsds_tm_rand_t *r, uint8_t *out,
                               size_t n);

#ifdef __cplusplus
}
#endif

#endif /* CCSDS_TM_H */
```


