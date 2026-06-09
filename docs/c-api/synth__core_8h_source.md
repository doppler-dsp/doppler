

# File synth\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**synth**](dir_135e4b6b03fee6eda2308471f560474b.md) **>** [**synth\_core.h**](synth__core_8h.md)

[Go to the documentation of this file](synth__core_8h.md)


```C++

#ifndef SYNTH_CORE_H
#define SYNTH_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "lo/lo_core.h"
#include "awgn/awgn_core.h"
#include "pn/pn_core.h"
#include <math.h> /* log10/powf/sqrtf in create_impl */
#ifdef __cplusplus
extern "C" {
#endif

enum {
    SYNTH_TONE = 0,  /* continuous-wave complex tone (LO)        */
    SYNTH_NOISE = 1, /* complex AWGN only                        */
    SYNTH_PN = 2,    /* BPSK-modulated PN m-sequence chips       */
    SYNTH_BPSK = 3,  /* BPSK over PN-sourced data bits           */
    SYNTH_QPSK = 4,  /* Gray-coded QPSK over PN-sourced data     */
};

/* snr >= this (dB) means "clean": no AWGN is generated at all (the common case
 * — a clean waveform shouldn't pay the noise cost). 100 dB SNR is the default
 * and is numerically clean anyway. Lower --snr to add noise. (type=noise always
 * generates AWGN regardless.) */
#define SYNTH_SNR_CLEAN 100.0

JM_FORCEINLINE uint64_t
synth_mls_poly(uint32_t n)
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

typedef struct {
    int wtype;
    int nsps;
    int sym_pos;
    float cur_re;
    float cur_im;
    lo_state_t * lo;
    awgn_state_t * awgn;
    pn_state_t * pn;
} synth_state_t;

synth_state_t *synth_create(int type, double fs, double freq, double snr, int snr_mode, uint32_t seed, int sps, int pn_length, uint64_t pn_poly, int lfsr);

void synth_destroy(synth_state_t *state);

void synth_reset(synth_state_t *state);

JM_FORCEINLINE JM_HOT float complex
synth_step(synth_state_t *state)
{
    if (state->wtype >= SYNTH_PN) {
        if (state->sym_pos == 0) {
            if (state->wtype == SYNTH_QPSK) {
                uint8_t b0 = pn_step(state->pn);
                uint8_t b1 = pn_step(state->pn);
                const float s = 0.70710678118654752f;
                state->cur_re = b0 ? -s : s;
                state->cur_im = b1 ? -s : s;
            } else { /* pn or bpsk: +-1 */
                uint8_t b = pn_step(state->pn);
                state->cur_re = b ? -1.0f : 1.0f;
                state->cur_im = 0.0f;
            }
        }
        if (++state->sym_pos >= state->nsps)
            state->sym_pos = 0;
    }
    float complex sym = state->cur_re + state->cur_im * I;
    float complex carrier = 1.0f + 0.0f * I;
    if (state->lo)
        lo_steps(state->lo, 1, &carrier);
    float complex noise = 0.0f + 0.0f * I;
    if (state->awgn)
        awgn_generate(state->awgn, 1, &noise);
    return sym * carrier + noise;
}

void synth_steps(
    synth_state_t *state,
    float complex          *output,
    size_t               n);

int synth_get_wtype(const synth_state_t *state);

void synth_set_wtype(synth_state_t *state, int val);

int synth_get_nsps(const synth_state_t *state);

void synth_set_nsps(synth_state_t *state, int val);

int synth_get_sym_pos(const synth_state_t *state);

void synth_set_sym_pos(synth_state_t *state, int val);

float synth_get_cur_re(const synth_state_t *state);

void synth_set_cur_re(synth_state_t *state, float val);

float synth_get_cur_im(const synth_state_t *state);

void synth_set_cur_im(synth_state_t *state, float val);



#ifdef __cplusplus
}
#endif

#endif /* SYNTH_CORE_H */
```


