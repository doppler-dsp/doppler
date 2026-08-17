

# File fec\_frame.h

[**File List**](files.md) **>** [**fec**](dir_df2a893a07d8c9ef377268dabdb4859f.md) **>** [**fec\_frame.h**](fec__frame_8h.md)

[Go to the documentation of this file](fec__frame_8h.md)


```C++

#ifndef FEC_FRAME_H
#define FEC_FRAME_H

#include "fec/fec_ccsds.h"
#include "fec/fec_rs.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    unsigned rs_depth;      
    int      randomise;     
    int      attach_asm;    
    int      convolutional; 
  } fec_frame_cfg_t;

  typedef struct
  {
    size_t first; 
    size_t n;     
  } fec_frame_span_t;

  typedef struct
  {
    size_t block_bits; 
    size_t cadu_bits;  
    size_t out_bits;   
    fec_frame_span_t marker;     
    fec_frame_span_t outer;      
    fec_frame_span_t randomised; 
    fec_frame_span_t inner;      
  } fec_frame_layout_t;

  size_t fec_frame_layout (const fec_frame_cfg_t *cfg, size_t frame_len,
                           fec_frame_layout_t *out);

  size_t fec_frame_encode (const fec_frame_cfg_t *cfg, conv_enc_t *conv,
                           const uint8_t *frame, size_t frame_len,
                           uint8_t *out, size_t max_out);

  typedef struct
  {
    size_t   frame_len;    
    unsigned rs_codewords; 
    unsigned rs_ok;        
  } fec_frame_rx_t;

  size_t fec_frame_decode (const fec_frame_cfg_t *cfg, const uint8_t *cadu,
                           size_t n_cadu, uint8_t *frame, size_t max_frame,
                           fec_frame_rx_t *rx);

#ifdef __cplusplus
}
#endif

#endif /* FEC_FRAME_H */
```


