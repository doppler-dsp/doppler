

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

JM_FORCEINLINE uint64_t
pn_mls_poly(uint32_t n)
{
    switch (n) {
    case 2: return 0x3u;
    case 3: return 0x5u;
    case 4: return 0x9u;
    case 5: return 0x12u;
    case 6: return 0x21u;
    case 7: return 0x41u;
    case 8: return 0x8Eu;
    case 9: return 0x108u;
    case 10: return 0x204u;
    case 11: return 0x402u;
    case 12: return 0x829u;
    case 13: return 0x100Du;
    case 14: return 0x2015u;
    case 15: return 0x4001u;
    case 16: return 0x8016u;
    case 17: return 0x10004u;
    case 18: return 0x20013u;
    case 19: return 0x40013u;
    case 20: return 0x80004u;
    case 21: return 0x100002u;
    case 22: return 0x200001u;
    case 23: return 0x400010u;
    case 24: return 0x80000Du;
    case 25: return 0x1000004u;
    case 26: return 0x2000023u;
    case 27: return 0x4000013u;
    case 28: return 0x8000004u;
    case 29: return 0x10000002u;
    case 30: return 0x20000029u;
    case 31: return 0x40000004u;
    case 32: return 0x80000057u;
    case 33: return 0x100000029ull;
    case 34: return 0x200000073ull;
    case 35: return 0x400000002ull;
    case 36: return 0x80000003Bull;
    case 37: return 0x100000001Full;
    case 38: return 0x2000000031ull;
    case 39: return 0x4000000008ull;
    case 40: return 0x800000001Cull;
    case 41: return 0x10000000004ull;
    case 42: return 0x2000000001Full;
    case 43: return 0x4000000002Cull;
    case 44: return 0x80000000032ull;
    case 45: return 0x10000000000Dull;
    case 46: return 0x200000000097ull;
    case 47: return 0x400000000010ull;
    case 48: return 0x80000000005Bull;
    case 49: return 0x1000000000038ull;
    case 50: return 0x200000000000Eull;
    case 51: return 0x4000000000025ull;
    case 52: return 0x8000000000004ull;
    case 53: return 0x10000000000023ull;
    case 54: return 0x2000000000003Eull;
    case 55: return 0x40000000000023ull;
    case 56: return 0x8000000000004Aull;
    case 57: return 0x100000000000016ull;
    case 58: return 0x200000000000031ull;
    case 59: return 0x40000000000003Dull;
    case 60: return 0x800000000000001ull;
    case 61: return 0x1000000000000013ull;
    case 62: return 0x2000000000000034ull;
    case 63: return 0x4000000000000001ull;
    case 64: return 0x800000000000000Dull;
    default: return 0u;
    }
}


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

size_t pn_generate(pn_state_t *state, size_t n, uint8_t *out,
                   size_t max_out);
#ifdef __cplusplus
}
#endif

#endif /* PN_CORE_H */
```


