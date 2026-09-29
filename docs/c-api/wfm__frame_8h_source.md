

# File wfm\_frame.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_frame.h**](wfm__frame_8h.md)

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
    /* PN: dp_pn_create (poly, seed, reg_bits, lfsr) */
    uint64_t poly; 
    uint64_t seed;     
    uint32_t reg_bits; 
    int      lfsr;     
    /* GOLD: dp_gold_create (taps_a, seed_a, taps_b, seed_b, reg_bits) */
    uint64_t taps_a, seed_a, taps_b, seed_b;
  } wfm_seq_t;

#define WFM_FRAME_NAME_MAX 16
#define WFM_FRAME_MAX_FIELDS 16
#define WFM_FRAME_MAX_STAGES 8

#define WFM_FIELD_MAX_BITS 261120

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

  int dp_wfm_frame_field_index (const wfm_frame_desc_t *d, const char *name);

  int dp_wfm_frame_add_field (wfm_frame_desc_t *d, const char *name,
                           const wfm_seq_t *seq, size_t reps);

  int dp_wfm_frame_add_derived (wfm_frame_desc_t *d, const char *name,
                             size_t bits);

  int dp_wfm_frame_add_stage (wfm_frame_desc_t *d, uint32_t kind,
                           const char *first, const char *last);

  int dp_wfm_frame_add_stage_at (wfm_frame_desc_t *d, uint32_t kind,
                                 unsigned first, unsigned n_fields);

  size_t dp_wfm_seq_bits (const wfm_seq_t *s, uint8_t *out, size_t max_out);

  size_t dp_wfm_field_render (const wfm_field_t *f, uint8_t *out,
                              size_t max_out);

  int dp_wfm_field_parse (const char *spec, wfm_field_t *field,
                          uint8_t **owned, const char **why);

  int dp_wfm_parse_u64 (const char *p, size_t n, uint64_t *v);

  size_t dp_wfm_field_format (const wfm_field_t *field, char *buf,
                              size_t cap);

  size_t dp_wfm_field_bits (const char *spec, uint8_t *out, size_t max_out,
                            const char **why);

  size_t dp_wfm_frame_assemble (const wfm_frame_desc_t *d,
                             const wfm_frame_ops_t *ops, uint8_t *out,
                             size_t max_out);

  int dp_wfm_frame_desc_layout (const wfm_frame_desc_t  *d,
                             wfm_frame_desc_layout_t *out);

  int dp_wfm_frame_fixed (wfm_frame_desc_t *d, const wfm_seq_t *preamble,
                          size_t reps, const wfm_seq_t *sync,
                          const wfm_seq_t *payload, int crc);

  size_t dp_wfm_dsss_desc_nchips (const wfm_frame_desc_t *d, size_t acq_len,
                               size_t acq_reps, size_t data_len);

  size_t dp_wfm_dsss_desc_chips (const wfm_frame_desc_t *d,
                              const wfm_frame_ops_t *ops,
                              const uint8_t *acq_code, size_t acq_len,
                              size_t acq_reps, const uint8_t *data_code,
                              size_t data_len, uint8_t *out, size_t max_out);

  int dp_wfm_frame_check (const wfm_frame_desc_t *d, const wfm_frame_ops_t *ops,
                       uint8_t *bits, wfm_frame_rx_t *rx);

  int dp_wfm_frame_desc_crc_ok (const wfm_frame_desc_t *d,
                             const uint8_t          *rx_bits);


#ifdef __cplusplus
}
#endif

#endif /* WFM_FRAME_H */
```


