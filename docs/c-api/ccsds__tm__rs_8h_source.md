

# File ccsds\_tm\_rs.h

[**File List**](files.md) **>** [**ccsds\_tm**](dir_c2a51186254da91e75ac1924b4969fdd.md) **>** [**ccsds\_tm\_rs.h**](ccsds__tm__rs_8h.md)

[Go to the documentation of this file](ccsds__tm__rs_8h.md)


```C++

#ifndef CCSDS_TM_RS_H
#define CCSDS_TM_RS_H

#include "rs/rs_core.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define CCSDS_TM_RS_N 255
#define CCSDS_TM_RS_K 223
#define CCSDS_TM_RS_E 16
#define CCSDS_TM_RS_2E 32
#define CCSDS_TM_RS_MAX_DEPTH 8

  extern const rs_code_t CCSDS_TM_RS;

  uint8_t ccsds_tm_rs_conv_to_dual (uint8_t u);

  uint8_t ccsds_tm_rs_dual_to_conv (uint8_t z);

  const uint8_t *ccsds_tm_rs_generator (void);

  int ccsds_tm_rs_codeword_ok (const uint8_t *codeword);

  int ccsds_tm_rs_decode (uint8_t *codeword);

  typedef struct
  {
    unsigned codewords;     
    unsigned corrected;     
    unsigned uncorrectable; 
    unsigned symbols;       
  } ccsds_tm_rs_block_rx_t;

  size_t ccsds_tm_rs_decode_block (uint8_t *block, unsigned depth,
                              ccsds_tm_rs_block_rx_t *rx);

  size_t ccsds_tm_rs_encode_block (const uint8_t *info, unsigned depth,
                              uint8_t *out);

  void ccsds_tm_rs_encode (const uint8_t *info, uint8_t *parity);

#ifdef __cplusplus
}
#endif

#endif /* CCSDS_TM_RS_H */
```


