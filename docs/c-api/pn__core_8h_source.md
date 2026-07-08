

# File pn\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**pn**](dir_70aeca018f85f00e17d8853ee6bd0cbb.md) **>** [**pn\_core.h**](pn__core_8h.md)

[Go to the documentation of this file](pn__core_8h.md)


```C++

#ifndef PN_CORE_H
#define PN_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

enum { PN_GALOIS = 0, PN_FIBONACCI = 1 };

typedef struct {
    uint64_t poly;     /* Galois feedback polynomial (taps) */
    uint64_t seed;     /* initial register state (for reset) */
    uint64_t reg;      /* current LFSR register */
    uint64_t mask;     /* (1 << length) - 1; all ones when length == 64 */
    int kind;          /* PN_GALOIS or PN_FIBONACCI */
    uint64_t fib_taps; /* Fibonacci feedback taps (canonical poly & mask) */
    uint32_t topshift; /* length-1: position the Fibonacci feedback enters */
} pn_state_t;

pn_state_t *pn_create(uint64_t poly, uint64_t seed, uint32_t length, int lfsr);

void pn_destroy(pn_state_t *state);

void pn_reset(pn_state_t *state);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Only the running LFSR register is serialized; poly / seed / mask / kind /
 * fib_taps / topshift are config restored by the constructor.
 * Envelope: [dp_state_hdr_t][u64 reg]. */
#define PN_STATE_MAGIC DP_FOURCC('P', 'N', '_', '_')
#define PN_STATE_VERSION 1u

size_t pn_state_bytes(const pn_state_t *state);
void pn_get_state(const pn_state_t *state, void *blob);
int pn_set_state(pn_state_t *state, const void *blob);

JM_FORCEINLINE uint8_t
pn_step(pn_state_t *state)
{
    uint8_t bit = (uint8_t)(state->reg & 1u);
    if (state->kind == PN_FIBONACCI) {
        uint64_t fb = (uint64_t)__builtin_parityll(state->reg & state->fib_taps);
        state->reg = (state->reg >> 1) | (fb << state->topshift);
    } else {
        state->reg >>= 1;
        if (bit)
            state->reg ^= state->poly;
        state->reg &= state->mask;
    }
    return bit;
}








size_t pn_generate_max_out(pn_state_t *state);

size_t pn_generate(pn_state_t *state, size_t n, uint8_t *out);
#ifdef __cplusplus
}
#endif

#endif /* PN_CORE_H */
```


