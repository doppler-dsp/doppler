

# File ccsds\_tm\_frame.h

[**File List**](files.md) **>** [**ccsds\_tm**](dir_c2a51186254da91e75ac1924b4969fdd.md) **>** [**ccsds\_tm\_frame.h**](ccsds__tm__frame_8h.md)

[Go to the documentation of this file](ccsds__tm__frame_8h.md)


```C++

#ifndef CCSDS_TM_FRAME_H
#define CCSDS_TM_FRAME_H

#include "ccsds_tm/ccsds_tm.h"
#include "ccsds_tm/ccsds_tm_rs.h"
#include "wfm/wfm_frame.h"

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
  } ccsds_tm_frame_cfg_t;

  typedef struct
  {
    size_t first; 
    size_t n;     
  } ccsds_tm_frame_span_t;

  typedef struct
  {
    size_t block_bits; 
    size_t cadu_bits;  
    size_t out_bits; 
    ccsds_tm_frame_span_t marker;     
    ccsds_tm_frame_span_t outer;
    ccsds_tm_frame_span_t randomised;
    ccsds_tm_frame_span_t inner;
  } ccsds_tm_frame_layout_t;

  size_t ccsds_tm_frame_layout (const ccsds_tm_frame_cfg_t *cfg,
                               size_t              frame_len,
                           ccsds_tm_frame_layout_t *out);

  size_t ccsds_tm_frame_encode (const ccsds_tm_frame_cfg_t *cfg,
                               conv_enc_t               *conv,
                           const uint8_t *frame, size_t frame_len,
                           uint8_t *out, size_t max_out);

  typedef struct
  {
    size_t   frame_len;    
    unsigned rs_codewords; 
    unsigned rs_ok;        
    unsigned rs_corrected; 
    unsigned rs_symbols;   
  } ccsds_tm_frame_rx_t;

  size_t ccsds_tm_frame_decode (const ccsds_tm_frame_cfg_t *cfg,
                               const uint8_t            *cadu,
                           size_t n_cadu, uint8_t *frame, size_t max_frame,
                           ccsds_tm_frame_rx_t *rx);

  int ccsds_tm_frame_describe (const ccsds_tm_frame_cfg_t *cfg,
                               size_t frame_len, const uint8_t *frame_bits,
                               wfm_frame_desc_t *out);

  typedef struct
  {
    int attach_asm;
    const uint8_t *preamble;
    size_t         preamble_len;
    size_t         preamble_reps;
    const uint8_t *sync;
    size_t         sync_len;
    const uint8_t *payload;
    size_t         payload_len;
    int crc;
    unsigned rs_depth;
    int randomise;
    int convolutional;
  } ccsds_tm_frame_spec_t;

  int ccsds_tm_frame_desc_of (const ccsds_tm_frame_spec_t *s,
                              wfm_frame_desc_t *d);

  void ccsds_tm_frame_ops (wfm_frame_ops_t *out, conv_enc_t *conv);

#ifdef __cplusplus
}
#endif

#endif /* CCSDS_TM_FRAME_H */
```


