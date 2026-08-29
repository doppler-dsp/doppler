

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

#define WFM_FRAME_NAME_MAX 16
#define WFM_FRAME_MAX_FIELDS 16
#define WFM_FRAME_MAX_STAGES 8

  typedef struct
  {
    size_t first; 
    size_t n;     
  } wfm_frame_span_t;

  typedef struct
  {
    char name[WFM_FRAME_NAME_MAX];

    wfm_seq_t seq;  
    size_t    reps; 
    size_t    bits; 
    unsigned derived_by;
  } wfm_field_t;

  typedef enum
  {
    WFM_STAGE_CRC16     = 0, 
    WFM_STAGE_RS        = 1, 
    WFM_STAGE_RANDOMISE = 2, 
    WFM_STAGE_CONV      = 3, 
    WFM_STAGE_INTERLEAVE = 4,

    WFM_STAGE_USER = 0x1000u
  } wfm_stage_kind_t;

  typedef struct
  {
    uint32_t kind;
    unsigned first_field; 
    unsigned         n_fields;    
    unsigned         depth;       
    unsigned unit_bits;

    unsigned emit_num, emit_den;
  } wfm_stage_t;

  typedef struct
  {
    wfm_field_t field[WFM_FRAME_MAX_FIELDS];
    unsigned    n_fields;
    wfm_stage_t stage[WFM_FRAME_MAX_STAGES];
    unsigned    n_stages;
  } wfm_frame_desc_t;

  typedef struct
  {
    size_t   field_off[WFM_FRAME_MAX_FIELDS];  
    size_t   field_bits[WFM_FRAME_MAX_FIELDS]; 
    unsigned n_fields;

    wfm_frame_span_t stage[WFM_FRAME_MAX_STAGES]; 
    unsigned   n_stages;

    size_t frame_bits; 
    size_t out_bits;   
  } wfm_frame_desc_layout_t;

  typedef struct
  {
    unsigned units;     
    unsigned ok;        
    unsigned corrected; 
    unsigned symbols;   
    int      checked;   
  } wfm_frame_stage_rx_t;

  typedef struct
  {
    wfm_frame_stage_rx_t stage[WFM_FRAME_MAX_STAGES];
    unsigned             n_stages;
    unsigned             checked; 
  } wfm_frame_rx_t;

  typedef struct
  {
    uint32_t kind;

    int (*in_unit) (const wfm_stage_t *st, uint8_t *bits, size_t n,
                    void *user);

    size_t (*emit) (const wfm_stage_t *st, const uint8_t *in, size_t n,
                    uint8_t *out, size_t max_out, void *user);

    int (*undo) (const wfm_stage_t *st, uint8_t *bits, size_t n,
                 wfm_frame_stage_rx_t *rx, void *user);
  } wfm_stage_op_t;

  typedef struct
  {
    const wfm_stage_op_t *op;   
    unsigned              n_op; 
    void                 *user; 
  } wfm_frame_ops_t;

  int wfm_frame_field_index (const wfm_frame_desc_t *d, const char *name);

  int wfm_frame_add_field (wfm_frame_desc_t *d, const char *name,
                           const wfm_seq_t *seq, size_t reps);

  int wfm_frame_add_derived (wfm_frame_desc_t *d, const char *name,
                             size_t bits);

  int wfm_frame_add_stage (wfm_frame_desc_t *d, uint32_t kind,
                           const char *first, const char *last);

  size_t wfm_frame_assemble (const wfm_frame_desc_t *d,
                             const wfm_frame_ops_t *ops, uint8_t *out,
                             size_t max_out);

  int wfm_frame_desc_layout (const wfm_frame_desc_t  *d,
                             wfm_frame_desc_layout_t *out);

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

  enum
  {
    WFM_FRAME_FIELD_PREAMBLE = 0,
    WFM_FRAME_FIELD_SYNC     = 1,
    WFM_FRAME_FIELD_PAYLOAD  = 2,
    WFM_FRAME_FIELD_CRC      = 3
  };

  int wfm_frame_describe (const wfm_frame_t *f, wfm_frame_desc_t *out);

  size_t wfm_frame_nbits (const wfm_frame_t *f);

  int wfm_frame_layout (const wfm_frame_t *f, wfm_frame_layout_t *out);

  size_t wfm_frame_bits (const wfm_frame_t *f, uint8_t *out, size_t max_out);

  size_t wfm_dsss_desc_nchips (const wfm_frame_desc_t *d, size_t acq_len,
                               size_t acq_reps, size_t data_len);

  size_t wfm_dsss_desc_chips (const wfm_frame_desc_t *d,
                              const wfm_frame_ops_t *ops,
                              const uint8_t *acq_code, size_t acq_len,
                              size_t acq_reps, const uint8_t *data_code,
                              size_t data_len, uint8_t *out, size_t max_out);

  int wfm_frame_check (const wfm_frame_desc_t *d, const wfm_frame_ops_t *ops,
                       uint8_t *bits, wfm_frame_rx_t *rx);

  int wfm_frame_desc_crc_ok (const wfm_frame_desc_t *d,
                             const uint8_t          *rx_bits);

  int wfm_frame_crc_ok (const wfm_frame_t *f, const uint8_t *rx_bits);

#ifdef __cplusplus
}
#endif

#endif /* WFM_FRAME_H */
```


