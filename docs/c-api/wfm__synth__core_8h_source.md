

# File wfm\_synth\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm\_synth**](dir_0493917d169dff974fa9eaf690c8d4c9.md) **>** [**wfm\_synth\_core.h**](wfm__synth__core_8h.md)

[Go to the documentation of this file](wfm__synth__core_8h.md)


```C++

#ifndef WFM_SYNTH_CORE_H
#define WFM_SYNTH_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "fir/fir_core.h"
#include "lo/lo_core.h"
#include "awgn/awgn_core.h"
#include "pn/pn_core.h"
#include <math.h> /* log10/powf/sqrtf in create_impl */
#ifdef __cplusplus
extern "C" {
#endif

enum {
    WFM_SYNTH_TONE = 0,  /* continuous-wave complex tone (LO)        */
    WFM_SYNTH_NOISE = 1, /* complex AWGN only                        */
    WFM_SYNTH_PN = 2,    /* BPSK-modulated PN m-sequence chips       */
    WFM_SYNTH_BPSK = 3,  /* BPSK over PN-sourced data bits           */
    WFM_SYNTH_QPSK = 4,  /* Gray-coded QPSK over PN-sourced data     */
    WFM_SYNTH_CHIRP = 5, /* linear-FM sweep f_start→f_end (no symbols) */
    WFM_SYNTH_BITS = 6,  /* user bit pattern, oversampled + cycled    */
};

/* snr >= this (dB) means "clean": no AWGN is generated at all (the common case
 * — a clean waveform shouldn't pay the noise cost). 100 dB SNR is the default
 * and is numerically clean anyway. Lower --snr to add noise. (type=noise always
 * generates AWGN regardless.) */
#define WFM_SYNTH_SNR_CLEAN 100.0

JM_FORCEINLINE uint64_t
wfm_synth_mls_poly(uint32_t n)
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
    double chirp_f0;
    double chirp_fend;
    double chirp_k;
    double chirp_ph;
    size_t chirp_n;
    size_t chirp_span;
    uint8_t * bits;
    size_t n_bits;
    size_t bit_idx;
    int bit_mod;
    fir_state_t * fir;
    lo_state_t * lo;
    awgn_state_t * awgn;
    pn_state_t * pn;
} wfm_synth_state_t;

wfm_synth_state_t *wfm_synth_create(int type, double fs, double freq, double snr, int snr_mode, uint32_t seed, int sps, int pn_length, uint64_t pn_poly, int lfsr, double f_end);

void wfm_synth_set_chirp_span(wfm_synth_state_t *state, size_t span);

int wfm_synth_set_bits(wfm_synth_state_t *state, const uint8_t *bits, size_t n,
                       int modulation);

int wfm_synth_set_rrc(wfm_synth_state_t *state, const float *taps,
                      size_t ntaps);

void wfm_synth_destroy(wfm_synth_state_t *state);

void wfm_synth_reset(wfm_synth_state_t *state);

JM_FORCEINLINE JM_HOT float complex
wfm_synth_step(wfm_synth_state_t *state)
{
    float complex sym;
    if (state->wtype == WFM_SYNTH_BITS) {
        /* User bit pattern, oversampled sps and cycled to fill the request. The
         * symbol latch mirrors the PN path but sources bits from bits[bit_idx]
         * instead of the LFSR; bit_mod picks the mapping. */
        if (state->sym_pos == 0 && state->bits && state->n_bits) {
            if (state->bit_mod == 2) { /* qpsk: 2 bits/symbol, Gray-mapped */
                uint8_t b0 = state->bits[state->bit_idx];
                uint8_t b1 = state->bits[(state->bit_idx + 1) % state->n_bits];
                const float s = 0.70710678118654752f;
                state->cur_re = b0 ? -s : s;
                state->cur_im = b1 ? -s : s;
                state->bit_idx = (state->bit_idx + 2) % state->n_bits;
            } else if (state->bit_mod == 1) { /* bpsk: 0->+1, 1->-1 */
                state->cur_re = state->bits[state->bit_idx] ? -1.0f : 1.0f;
                state->cur_im = 0.0f;
                state->bit_idx = (state->bit_idx + 1) % state->n_bits;
            } else { /* none: unmodulated 0/1 amplitude */
                state->cur_re = state->bits[state->bit_idx] ? 1.0f : 0.0f;
                state->cur_im = 0.0f;
                state->bit_idx = (state->bit_idx + 1) % state->n_bits;
            }
        }
        if (++state->sym_pos >= state->nsps)
            state->sym_pos = 0;
        sym = state->cur_re + state->cur_im * I; /* bits: no pulse shaping */
    } else if (state->wtype >= WFM_SYNTH_PN && state->wtype <= WFM_SYNTH_QPSK) {
        if (state->sym_pos == 0) {
            if (state->wtype == WFM_SYNTH_QPSK) {
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
        if (state->fir) {
            /* RRC pulse shaping: feed the symbol-rate impulse train (the held
             * symbol at a boundary, zero between) through the matched FIR. The
             * FIR carries its delay line across calls, so this is chunk-invariant
             * — step() and the block path agree bit-for-bit. */
            float complex imp = (state->sym_pos == 0)
                                    ? (state->cur_re + state->cur_im * I)
                                    : (0.0f + 0.0f * I);
            fir_execute(state->fir, &imp, 1, &sym);
        } else {
            sym = state->cur_re + state->cur_im * I; /* rect sample-and-hold */
        }
        if (++state->sym_pos >= state->nsps)
            state->sym_pos = 0;
    } else {
        sym = state->cur_re + state->cur_im * I;
    }
    float complex carrier = 1.0f + 0.0f * I;
    if (state->lo) {
        lo_steps(state->lo, 1, &carrier);
    } else if (state->wtype == WFM_SYNTH_CHIRP) {
        /* Sweeping carrier: f(n) = f0 + k*n (normalised cycles/sample), held at
         * f_end once the span is reached. Phase accumulates in cycles, wrapped to
         * [0,1) each step so the double keeps precision over a long sweep. The
         * fused sym*carrier + noise below is the *same* expression the tone path
         * (and wfm_synth_steps) uses, so step()/steps() stay byte-identical. */
        double nf = (state->chirp_span && state->chirp_n >= state->chirp_span)
                        ? (double)state->chirp_span
                        : (double)state->chirp_n;
        double w   = state->chirp_f0 + state->chirp_k * nf;
        carrier    = cexpf((float)(6.283185307179586 * state->chirp_ph) * I);
        state->chirp_ph += w;
        state->chirp_ph -= floor(state->chirp_ph);
        state->chirp_n++;
    }
    float complex noise = 0.0f + 0.0f * I;
    if (state->awgn)
        awgn_generate(state->awgn, 1, &noise);
    return sym * carrier + noise;
}

void wfm_synth_steps(
    wfm_synth_state_t *state,
    float complex          *output,
    size_t               n);

int wfm_synth_get_wtype(const wfm_synth_state_t *state);

void wfm_synth_set_wtype(wfm_synth_state_t *state, int val);

int wfm_synth_get_nsps(const wfm_synth_state_t *state);

void wfm_synth_set_nsps(wfm_synth_state_t *state, int val);

int wfm_synth_get_sym_pos(const wfm_synth_state_t *state);

void wfm_synth_set_sym_pos(wfm_synth_state_t *state, int val);

float wfm_synth_get_cur_re(const wfm_synth_state_t *state);

void wfm_synth_set_cur_re(wfm_synth_state_t *state, float val);

float wfm_synth_get_cur_im(const wfm_synth_state_t *state);

void wfm_synth_set_cur_im(wfm_synth_state_t *state, float val);



#ifdef __cplusplus
}
#endif

#endif /* WFM_SYNTH_CORE_H */
```


