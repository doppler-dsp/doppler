

# File gold\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**gold**](dir_3809e94b548bea726f665dae5e933c35.md) **>** [**gold\_core.h**](gold__core_8h.md)

[Go to the documentation of this file](gold__core_8h.md)


```C++

#ifndef DP_GOLD_CORE_H
#define DP_GOLD_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t reg_a;   /* Register A: current LFSR register */
    uint64_t reg_b;   /* Register B: current LFSR register */
    uint64_t taps_a;  /* Register A: fixed feedback-tap mask (bit k = stage k+1) */
    uint64_t taps_b;  /* Register B: fixed feedback-tap mask (bit k = stage k+1) */
    uint64_t seed_a;  /* Register A: initial value (for reset); user-dependent */
    uint64_t seed_b;  /* Register B: initial value (for reset); fixed by CCSDS */
    uint64_t mask;    /* (1 << length) - 1; all ones when length == 64 */
    uint32_t length;  /* register width in bits (stage count); CCSDS uses 10 */
} dp_gold_state_t;

dp_gold_state_t *dp_gold_create(uint64_t taps_a, uint64_t seed_a, uint64_t taps_b,
                           uint64_t seed_b, uint32_t length);

void dp_gold_destroy(dp_gold_state_t *state);

void dp_gold_reset(dp_gold_state_t *state);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Only the two running LFSR registers are serialized; taps / seeds / mask /
 * length are config restored by the constructor.
 * Envelope: [dp_state_hdr_t][u64 reg_a][u64 reg_b]. */
#define GOLD_STATE_MAGIC DP_FOURCC('G', 'O', 'L', 'D')
#define GOLD_STATE_VERSION 1u

size_t dp_gold_state_bytes(const dp_gold_state_t *state);
void dp_gold_get_state(const dp_gold_state_t *state, void *blob);
int dp_gold_set_state(dp_gold_state_t *state, const void *blob);

JM_FORCEINLINE uint8_t
gold_step(dp_gold_state_t *state)
{
    uint64_t a = state->reg_a;
    uint64_t b = state->reg_b;
    uint32_t top = state->length - 1u;
    uint8_t out = (uint8_t)(((a >> top) ^ (b >> top)) & 1u);
    uint64_t fb_a = (uint64_t)__builtin_parityll(a & state->taps_a);
    uint64_t fb_b = (uint64_t)__builtin_parityll(b & state->taps_b);
    state->reg_a = ((a << 1) | fb_a) & state->mask;
    state->reg_b = ((b << 1) | fb_b) & state->mask;
    return out;
}

size_t dp_gold_generate_max_out(dp_gold_state_t *state);

size_t dp_gold_generate(dp_gold_state_t *state, size_t n, uint8_t *out,
                     size_t max_out);
#ifdef __cplusplus
}
#endif

#endif /* GOLD_CORE_H */
```


