

# File fec\_ccsds.h

[**File List**](files.md) **>** [**fec**](dir_df2a893a07d8c9ef377268dabdb4859f.md) **>** [**fec\_ccsds.h**](fec__ccsds_8h.md)

[Go to the documentation of this file](fec__ccsds_8h.md)


```C++

#ifndef FEC_CCSDS_H
#define FEC_CCSDS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define FEC_CCSDS_ASM 0x1ACFFC1DuL

#define FEC_CCSDS_ASM_BITS 32

#define FEC_CONV_K 7

  void fec_ccsds_asm_bits (uint8_t *out);

  void fec_ccsds_randomise (uint8_t *bits, size_t n);

  typedef struct
  {
    uint8_t reg; 
  } fec_conv_t;

  void fec_conv_init (fec_conv_t *s);

  size_t fec_conv_encode (fec_conv_t *s, const uint8_t *in, size_t n,
                          uint8_t *out);

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


