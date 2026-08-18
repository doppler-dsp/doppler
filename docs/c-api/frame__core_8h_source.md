

# File frame\_core.h

[**File List**](files.md) **>** [**frame**](dir_00858a83d5a24a6fcf61a222bafb8b7f.md) **>** [**frame\_core.h**](frame__core_8h.md)

[Go to the documentation of this file](frame__core_8h.md)


```C++

#ifndef FRAME_CORE_H
#define FRAME_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "pn/pn_core.h"
#include "gold/gold_core.h"
#include "wfm/wfm_frame.h" /* the descriptor and its layout — the one SSOT */
#include "conv/conv_core.h"
#include "rs/rs_core.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    wfm_frame_desc_t d;
    wfm_frame_desc_layout_t dl;
    wfm_frame_t f;
    wfm_frame_layout_t l;
    int named;
    uint8_t *own[WFM_FRAME_MAX_FIELDS];
    uint8_t *one;
/*<<property_struct_fields>>*/
  size_t nbits;
} frame_state_t;

frame_state_t *frame_create(int preamble_kind, const uint8_t *preamble, size_t preamble_len, size_t preamble_nbits, size_t preamble_reps, uint64_t preamble_poly, uint64_t preamble_seed, uint32_t preamble_reg_bits, int preamble_lfsr, uint64_t preamble_taps_a, uint64_t preamble_seed_a, uint64_t preamble_taps_b, uint64_t preamble_seed_b, int sync_kind, const uint8_t *sync, size_t sync_len, size_t sync_nbits, uint64_t sync_poly, uint64_t sync_seed, uint32_t sync_reg_bits, int sync_lfsr, uint64_t sync_taps_a, uint64_t sync_seed_a, uint64_t sync_taps_b, uint64_t sync_seed_b, int payload_kind, const uint8_t *payload, size_t payload_len, size_t payload_nbits, uint64_t payload_poly, uint64_t payload_seed, uint32_t payload_reg_bits, int payload_lfsr, uint64_t payload_taps_a, uint64_t payload_seed_a, uint64_t payload_taps_b, uint64_t payload_seed_b, int crc);

void frame_destroy(frame_state_t *state);

size_t frame_bits_max_out(frame_state_t *state, size_t n);

size_t frame_bits(frame_state_t *state, size_t n, uint8_t *out, size_t max_out);

wfm_frame_layout_t frame_layout(frame_state_t *state);

int frame_crc_ok(frame_state_t *state, const uint8_t *rx_bits, size_t rx_bits_len);

frame_state_t *frame_create_desc(int preamble_kind, const uint8_t *preamble, size_t preamble_len, size_t preamble_nbits, size_t preamble_reps, uint64_t preamble_poly, uint64_t preamble_seed, uint32_t preamble_reg_bits, int preamble_lfsr, uint64_t preamble_taps_a, uint64_t preamble_seed_a, uint64_t preamble_taps_b, uint64_t preamble_seed_b, int sync_kind, const uint8_t *sync, size_t sync_len, size_t sync_nbits, uint64_t sync_poly, uint64_t sync_seed, uint32_t sync_reg_bits, int sync_lfsr, uint64_t sync_taps_a, uint64_t sync_seed_a, uint64_t sync_taps_b, uint64_t sync_seed_b, int payload_kind, const uint8_t *payload, size_t payload_len, size_t payload_nbits, uint64_t payload_poly, uint64_t payload_seed, uint32_t payload_reg_bits, int payload_lfsr, uint64_t payload_taps_a, uint64_t payload_seed_a, uint64_t payload_taps_b, uint64_t payload_seed_b, int crc);

int frame_add_field(frame_state_t *state, const uint8_t *lit, size_t lit_len,
                    int kind, size_t gen_len, size_t reps, uint64_t poly,
                    uint64_t seed, uint32_t reg_bits, int lfsr,
                    uint64_t taps_a, uint64_t seed_a, uint64_t taps_b,
                    uint64_t seed_b, uint32_t derived_by,
                    size_t derived_bits);

int frame_add_stage(frame_state_t *state, int kind, uint32_t first_field,
                    uint32_t n_fields, uint32_t depth, uint32_t emit_num,
                    uint32_t emit_den);

int frame_build(frame_state_t *state);

typedef struct {
    int      passed;    
    uint32_t stages;    
    uint32_t checked;   
    uint32_t units;     
    uint32_t ok;        
    uint32_t corrected; 
    uint32_t symbols;   
} frame_check_t;

frame_check_t frame_check(frame_state_t *state, const uint8_t *rx_bits, size_t rx_bits_len);

size_t frame_n_fields(frame_state_t *state);

size_t frame_n_stages(frame_state_t *state);

size_t frame_field_off(frame_state_t *state, size_t i);

size_t frame_field_bits(frame_state_t *state, size_t i);

size_t frame_stage_first(frame_state_t *state, size_t i);

size_t frame_stage_bits(frame_state_t *state, size_t i);

#ifdef __cplusplus
}
#endif

#endif /* FRAME_CORE_H */
```


