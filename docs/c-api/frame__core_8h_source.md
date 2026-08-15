

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
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    wfm_frame_t f;
    wfm_frame_layout_t l;
    uint8_t *preamble_own, *sync_own, *payload_own;
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
#ifdef __cplusplus
}
#endif

#endif /* FRAME_CORE_H */
```


