

# File frame\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**frame**](dir_f1fb4d4532bf7057e52e2ed064f78108.md) **>** [**frame\_core.h**](frame__core_8h.md)

[Go to the documentation of this file](frame__core_8h.md)


```C++

#ifndef DP_FRAME_CORE_H
#define DP_FRAME_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/pn/pn_core.h"
#include "doppler/gold/gold_core.h"
#include "doppler/wfm/wfm_frame.h" /* the descriptor and its layout — the one SSOT */
#include "doppler/conv/conv_core.h"
#include "doppler/rs/rs_core.h"
#include "doppler/cvt/cvt_core.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    wfm_frame_desc_t d;
    wfm_frame_desc_layout_t dl;
    int rx_checked;
    int rx_units;
    int rx_ok;
    int rx_symbols;
    wfm_frame_t f;
    wfm_frame_layout_t l;
    int named;
    uint8_t *own[WFM_FRAME_MAX_FIELDS];
    uint8_t *one;
/*<<property_struct_fields>>*/
  size_t nbits;
} dp_frame_state_t;

dp_frame_state_t *dp_frame_create(int preamble_kind, const uint8_t *preamble, size_t preamble_len, size_t preamble_nbits, size_t preamble_reps, uint64_t preamble_poly, uint64_t preamble_seed, uint32_t preamble_reg_bits, int preamble_lfsr, uint64_t preamble_taps_a, uint64_t preamble_seed_a, uint64_t preamble_taps_b, uint64_t preamble_seed_b, int sync_kind, const uint8_t *sync, size_t sync_len, size_t sync_nbits, uint64_t sync_poly, uint64_t sync_seed, uint32_t sync_reg_bits, int sync_lfsr, uint64_t sync_taps_a, uint64_t sync_seed_a, uint64_t sync_taps_b, uint64_t sync_seed_b, int payload_kind, const uint8_t *payload, size_t payload_len, size_t payload_nbits, uint64_t payload_poly, uint64_t payload_seed, uint32_t payload_reg_bits, int payload_lfsr, uint64_t payload_taps_a, uint64_t payload_seed_a, uint64_t payload_taps_b, uint64_t payload_seed_b, int crc);

void dp_frame_destroy(dp_frame_state_t *state);

size_t dp_frame_bits_max_out(dp_frame_state_t *state, size_t n);

size_t dp_frame_bits(dp_frame_state_t *state, size_t n, uint8_t *out, size_t max_out);

wfm_frame_layout_t dp_frame_layout(dp_frame_state_t *state);

int dp_frame_crc_ok(dp_frame_state_t *state, const uint8_t *rx_bits, size_t rx_bits_len);

dp_frame_state_t *frame_create_desc(int preamble_kind, const uint8_t *preamble, size_t preamble_len, size_t preamble_nbits, size_t preamble_reps, uint64_t preamble_poly, uint64_t preamble_seed, uint32_t preamble_reg_bits, int preamble_lfsr, uint64_t preamble_taps_a, uint64_t preamble_seed_a, uint64_t preamble_taps_b, uint64_t preamble_seed_b, int sync_kind, const uint8_t *sync, size_t sync_len, size_t sync_nbits, uint64_t sync_poly, uint64_t sync_seed, uint32_t sync_reg_bits, int sync_lfsr, uint64_t sync_taps_a, uint64_t sync_seed_a, uint64_t sync_taps_b, uint64_t sync_seed_b, int payload_kind, const uint8_t *payload, size_t payload_len, size_t payload_nbits, uint64_t payload_poly, uint64_t payload_seed, uint32_t payload_reg_bits, int payload_lfsr, uint64_t payload_taps_a, uint64_t payload_seed_a, uint64_t payload_taps_b, uint64_t payload_seed_b, int crc);

int dp_frame_add_field(dp_frame_state_t *state, const uint8_t *lit, size_t lit_len,
                    int kind, size_t gen_len, size_t reps, uint64_t poly,
                    uint64_t seed, uint32_t reg_bits, int lfsr,
                    uint64_t taps_a, uint64_t seed_a, uint64_t taps_b,
                    uint64_t seed_b, uint32_t derived_by,
                    size_t derived_bits);

int dp_frame_add_stage(dp_frame_state_t *state, int kind, uint32_t first_field,
                    uint32_t n_fields, uint32_t depth, uint32_t emit_num,
                    uint32_t emit_den, uint32_t unit_bits);

int dp_frame_build(dp_frame_state_t *state);

int dp_frame_field_index(dp_frame_state_t *state, const char *name);

int dp_frame_name_field(dp_frame_state_t *state, uint32_t index, const char *name);

int dp_frame_add_derived(dp_frame_state_t *state, const char *name, size_t bits);

int dp_frame_add_hex(dp_frame_state_t *state, const char *name, const char *hex,
                  size_t reps);

int dp_frame_add_value(dp_frame_state_t *state, const char *name, uint64_t value,
                    uint32_t bits, size_t reps);

int dp_frame_add_stage_over(dp_frame_state_t *state, int kind, const char *first,
                         const char *last, uint32_t depth,
                         uint32_t unit_bits);


typedef struct {
    int      passed;    
    uint32_t stages;    
    uint32_t checked;   
    uint32_t units;     
    uint32_t ok;        
    uint32_t corrected; 
    uint32_t symbols;   
} frame_check_t;

frame_check_t dp_frame_check(dp_frame_state_t *state, const uint8_t *rx_bits, size_t rx_bits_len);

size_t dp_frame_deframe(dp_frame_state_t *state, const uint8_t *rx_bits, size_t rx_bits_len, uint8_t *out, size_t max_out);

size_t dp_frame_deframe_max_out(dp_frame_state_t *state, size_t rx_bits_len);


size_t dp_frame_n_fields(dp_frame_state_t *state);

size_t dp_frame_n_stages(dp_frame_state_t *state);

size_t dp_frame_field_off(dp_frame_state_t *state, size_t i);

size_t dp_frame_field_bits(dp_frame_state_t *state, size_t i);

size_t dp_frame_stage_first(dp_frame_state_t *state, size_t i);

size_t dp_frame_stage_bits(dp_frame_state_t *state, size_t i);

#ifdef __cplusplus
}
#endif

#endif /* FRAME_CORE_H */
```


