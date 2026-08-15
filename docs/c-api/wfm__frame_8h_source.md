

# File wfm\_frame.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_frame.h**](wfm__frame_8h.md)

[Go to the documentation of this file](wfm__frame_8h.md)


```C++

#ifndef WFM_FRAME_H
#define WFM_FRAME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define WFM_FRAME_CRC_BITS 16u

  typedef enum
  {
    WFM_SEQ_LITERAL = 0, 
    WFM_SEQ_PN      = 1, 
    WFM_SEQ_GOLD    = 2, 
    WFM_SEQ_DOTTED  = 3  
  } wfm_seq_kind_t;

  typedef struct
  {
    wfm_seq_kind_t kind;
    size_t         len; 
    const uint8_t *bits; 
    /* PN: pn_create (poly, seed, reg_bits, lfsr) */
    uint64_t poly; 
    uint64_t seed;     
    uint32_t reg_bits; 
    int      lfsr;     
    /* GOLD: gold_create (taps_a, seed_a, taps_b, seed_b, reg_bits) */
    uint64_t taps_a, seed_a, taps_b, seed_b;
  } wfm_seq_t;

  typedef struct
  {
    wfm_seq_t preamble;      
    size_t    preamble_reps; 
    wfm_seq_t sync;          
    wfm_seq_t payload;
    int       crc; 
  } wfm_frame_t;

  typedef struct
  {
    size_t preamble_off, preamble_bits;
    size_t sync_off, sync_bits;
    size_t payload_off, payload_bits;
    size_t crc_off, crc_bits; 
    size_t total_bits;
  } wfm_frame_layout_t;

  size_t wfm_frame_nbits (const wfm_frame_t *f);

  int wfm_frame_layout (const wfm_frame_t *f, wfm_frame_layout_t *out);

  size_t wfm_frame_bits (const wfm_frame_t *f, uint8_t *out, size_t max_out);

  int wfm_frame_crc_ok (const wfm_frame_t *f, const uint8_t *rx_bits);

#ifdef __cplusplus
}
#endif

#endif /* WFM_FRAME_H */
```


