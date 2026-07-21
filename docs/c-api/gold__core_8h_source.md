

# File gold\_core.h

[**File List**](files.md) **>** [**gold**](dir_eaad5c90f79e5666c89030cb43ebb96d.md) **>** [**gold\_core.h**](gold__core_8h.md)

[Go to the documentation of this file](gold__core_8h.md)


```C++

#ifndef GOLD_CORE_H
#define GOLD_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
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
} gold_state_t;

gold_state_t *gold_create(uint64_t taps_a, uint64_t seed_a, uint64_t taps_b,
                           uint64_t seed_b, uint32_t length);

void gold_destroy(gold_state_t *state);

void gold_reset(gold_state_t *state);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Only the two running LFSR registers are serialized; taps / seeds / mask /
 * length are config restored by the constructor.
 * Envelope: [dp_state_hdr_t][u64 reg_a][u64 reg_b]. */
#define GOLD_STATE_MAGIC DP_FOURCC('G', 'O', 'L', 'D')
#define GOLD_STATE_VERSION 1u

size_t gold_state_bytes(const gold_state_t *state);
void gold_get_state(const gold_state_t *state, void *blob);
int gold_set_state(gold_state_t *state, const void *blob);

JM_FORCEINLINE uint8_t
gold_step(gold_state_t *state)
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

size_t gold_generate_max_out(gold_state_t *state);

size_t gold_generate(gold_state_t *state, size_t n, uint8_t *out);
#ifdef __cplusplus
}
#endif

#endif /* GOLD_CORE_H */
```


