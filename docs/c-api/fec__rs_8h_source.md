

# File fec\_rs.h

[**File List**](files.md) **>** [**fec**](dir_df2a893a07d8c9ef377268dabdb4859f.md) **>** [**fec\_rs.h**](fec__rs_8h.md)

[Go to the documentation of this file](fec__rs_8h.md)


```C++

#ifndef FEC_RS_H
#define FEC_RS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define FEC_RS_N 255
#define FEC_RS_K 223
#define FEC_RS_2E 32
#define FEC_RS_MAX_DEPTH 8

  uint8_t fec_rs_conv_to_dual (uint8_t u);

  uint8_t fec_rs_dual_to_conv (uint8_t z);

  const uint8_t *fec_rs_generator (void);

  int fec_rs_codeword_ok (const uint8_t *codeword);

  size_t fec_rs_encode_block (const uint8_t *info, unsigned depth,
                              uint8_t *out);

  void fec_rs_encode (const uint8_t *info, uint8_t *parity);

#ifdef __cplusplus
}
#endif

#endif /* FEC_RS_H */
```


