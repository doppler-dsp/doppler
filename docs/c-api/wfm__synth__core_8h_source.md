

# File wfm\_synth\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm\_synth**](dir_0493917d169dff974fa9eaf690c8d4c9.md) **>** [**wfm\_synth\_core.h**](wfm__synth__core_8h.md)

[Go to the documentation of this file](wfm__synth__core_8h.md)


```C++

#ifndef WFM_SYNTH_CORE_H
#define WFM_SYNTH_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "fir/fir_core.h"
#include "lo/lo_core.h"
#include "awgn/awgn_core.h"
#include "pn/pn_core.h"
#include "resamp/resamp_core.h"
#include <math.h> /* log10/powf/sqrtf in create_impl */
#include "gold/gold_core.h"
#include "mpsk/mpsk_core.h" /* mpsk_constellation — the ONE bit->symbol map */
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
    WFM_SYNTH_SYMBOLS
    = 7,                /* user complex-symbol stream, oversampled + cycled */
    WFM_SYNTH_DSSS = 8, /* two-code DSSS burst: repeated preamble +
                           spread frame, built by wfm_synth_set_dsss();
                           OR a continuous asynchronous stream when a
                           symbol_rate is supplied (wfm_synth_set_dsss_cont).
                           The two modes share this one type — a symbol_rate
                           discriminates, no tenth waveform type. */
};

enum {
    WFM_DSSS_DATA_NONE = 0, /* code-only: constant bit 0 -> the pure code    */
    WFM_DSSS_DATA_BITS = 1, /* a caller payload array, cycled mod n_bits      */
    WFM_DSSS_DATA_PRBS = 2, /* bits from the seeded PN LFSR (regenerable)     */
};

/* snr >= this (dB) means "clean": no AWGN is generated at all (the common case
 * — a clean waveform shouldn't pay the noise cost). 100 dB SNR is the default
 * and is numerically clean anyway. Lower --snr to add noise. (type=noise always
 * generates AWGN regardless.) */
#define WFM_SYNTH_SNR_CLEAN 100.0

JM_FORCEINLINE int
wfm_synth_bps (int type)
{
  return (type == WFM_SYNTH_QPSK) ? 2 : 1;
}

JM_FORCEINLINE double
wfm_synth_snr_over_fs (int mode, int bps, double span, double snr)
{
  double s = (span > 0.0) ? span : 1.0;
  if (mode == 2) /* Eb/No */
    return snr + 10.0 * log10 ((double)bps) - 10.0 * log10 (s);
  if (mode == 3) /* Es/No */
    return snr - 10.0 * log10 (s);
  return snr; /* over fs */
}

JM_FORCEINLINE uint64_t
wfm_synth_mls_poly(uint32_t n)
{
    return pn_mls_poly(n);
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
    float _Complex * symbols;
    size_t n_symbols;
    size_t sym_read_idx;
    /* continuous asynchronous DSSS (type=dsss + symbol_rate > 0):
       chips_per_symbol == 0 means burst mode (the fields below are unused). */
    double chips_per_symbol; /* config: chip_rate / symbol_rate (non-integer) */
    uint8_t * code;          /* config: spreading code (0/1), owned          */
    size_t n_code;           /* config: spreading code length in chips        */
    int data_mode;           /* config: WFM_DSSS_DATA_{NONE,BITS,PRBS}        */
    uint64_t chip_n;         /* running: chips emitted so far                 */
    uint64_t sym_idx;        /* running: current data-symbol index            */
    uint8_t cur_data;        /* running: data bit latched for this symbol      */
    fir_state_t * fir;       /* dense RRC FIR (non-power-of-two sps fallback)  */
    /* Polyphase RRC pulse shaper: a resamp interpolate-by-sps view over the
       RRC bank, replacing the dense fir (impulse-train + full FIR) with ~sps×
       fewer MACs. Built by wfm_synth_set_rrc when sps is a power of two; the
       dense `fir` is used otherwise. Exactly one of fir/shaper is ever set. */
    resamp_state_t * shaper;
    uint8_t primed;          /* running: shaper's sps-sample latency primed     */
    lo_state_t * lo;
    awgn_state_t * awgn;
    pn_state_t * pn;
} wfm_synth_state_t;

JM_FORCEINLINE float _Complex
wfm_synth_bit_symbol(wfm_synth_state_t *s)
{
    unsigned g = 0u;
    int      k;
    if (s->bit_mod <= 0) {
        float a    = s->bits[s->bit_idx] ? 1.0f : 0.0f;
        s->bit_idx = (s->bit_idx + 1) % s->n_bits;
        return a + 0.0f * I;
    }
    for (k = 0; k < s->bit_mod; k++) { /* MSB-first within the symbol */
        g          = (g << 1) | (unsigned)(s->bits[s->bit_idx] ? 1u : 0u);
        s->bit_idx = (s->bit_idx + 1) % s->n_bits;
    }
    return mpsk_constellation(g, 1 << s->bit_mod);
}

JM_FORCEINLINE float
wfm_synth_cont_dsss_chip(wfm_synth_state_t *s)
{
    uint64_t n   = s->chip_n;
    uint64_t sym = (uint64_t)((double)n / s->chips_per_symbol);
    if (n == 0 || sym != s->sym_idx) {
        s->sym_idx = sym;
        if (s->data_mode == WFM_DSSS_DATA_PRBS)
            s->cur_data = s->pn ? pn_step(s->pn) : 0u;
        else if (s->data_mode == WFM_DSSS_DATA_BITS)
            s->cur_data = (s->bits && s->n_bits)
                              ? (uint8_t)(s->bits[sym % s->n_bits] & 1u)
                              : 0u;
        else
            s->cur_data = 0u; /* code-only: the pure code, +code polarity */
    }
    uint8_t code_bit = (uint8_t)(s->code[n % s->n_code] & 1u);
    s->chip_n        = n + 1;
    return (code_bit ^ s->cur_data) ? -1.0f : 1.0f;
}

JM_FORCEINLINE float _Complex
wfm_synth_next_symbol(wfm_synth_state_t *s)
{
    const float q = 0.70710678118654752f; /* 1/sqrt(2) — QPSK leg */
    if (s->wtype == WFM_SYNTH_SYMBOLS) {
        float _Complex v = 0.0f + 0.0f * I;
        if (s->symbols && s->n_symbols) {
            v = s->symbols[s->sym_read_idx];
            s->sym_read_idx = (s->sym_read_idx + 1) % s->n_symbols;
        }
        return v;
    }
    if (s->wtype == WFM_SYNTH_BITS || s->wtype == WFM_SYNTH_DSSS) {
        if (s->chips_per_symbol > 0.0) /* continuous DSSS: lazy chip */
            return wfm_synth_cont_dsss_chip(s) + 0.0f * I;
        if (s->bits && s->n_bits) {
            return wfm_synth_bit_symbol(s);
        }
        return 0.0f + 0.0f * I;
    }
    /* pn / bpsk / qpsk: source symbols from the LFSR */
    if (s->wtype == WFM_SYNTH_QPSK) {
        uint8_t b0 = pn_step(s->pn);
        uint8_t b1 = pn_step(s->pn);
        return (b0 ? -q : q) + (b1 ? -q : q) * I;
    }
    uint8_t b = pn_step(s->pn);
    return (b ? -1.0f : 1.0f) + 0.0f * I;
}

JM_FORCEINLINE void
wfm_synth_shaper_prime(wfm_synth_state_t *s)
{
    size_t left = (size_t)s->nsps;
    while (left) {
        float _Complex syms[64], scratch[64];
        size_t pm = left < 64 ? left : 64;
        size_t need = resamp_interp_inputs_needed(s->shaper, pm);
        for (size_t k = 0; k < need; k++)
            syms[k] = wfm_synth_next_symbol(s);
        resamp_interp_fill(s->shaper, syms, scratch, pm);
        left -= pm;
    }
    s->primed = 1;
}

JM_FORCEINLINE void
wfm_synth_shape(wfm_synth_state_t *s, float _Complex *out, size_t m,
                float _Complex *syms)
{
    if (!s->primed)
        wfm_synth_shaper_prime(s);
    size_t need = resamp_interp_inputs_needed(s->shaper, m);
    for (size_t k = 0; k < need; k++)
        syms[k] = wfm_synth_next_symbol(s);
    resamp_interp_fill(s->shaper, syms, out, m);
}

wfm_synth_state_t *wfm_synth_create(int type, double fs, double freq, double snr, int snr_mode, uint32_t seed, int sps, int pn_length, uint64_t pn_poly, int lfsr, double f_end);

void wfm_synth_set_chirp_span(wfm_synth_state_t *state, size_t span);

int wfm_synth_set_bits(wfm_synth_state_t *state, const uint8_t *bits, size_t n,
                       int modulation);

int wfm_synth_set_dsss(wfm_synth_state_t *state, const uint8_t *acq_code,
                       size_t acq_len, size_t acq_reps,
                       const uint8_t *data_code, size_t data_len,
                       const uint8_t *sync, size_t sync_len,
                       const uint8_t *payload, size_t payload_len, int crc);

int wfm_synth_set_dsss_cont(wfm_synth_state_t *state, const uint8_t *code,
                            size_t code_len, double chips_per_symbol,
                            int data_mode, const uint8_t *data, size_t n_data);

int wfm_synth_set_symbols(wfm_synth_state_t *state,
                          const float _Complex *symbols, size_t n);

int wfm_synth_set_rrc(wfm_synth_state_t *state, const float *taps,
                      size_t ntaps);

void wfm_synth_destroy(wfm_synth_state_t *state);

void wfm_synth_reset(wfm_synth_state_t *state);

void wfm_synth_reseed_noise(wfm_synth_state_t *state, uint32_t seed);

void wfm_synth_noise_steps(wfm_synth_state_t *state, float complex *output,
                           size_t n);

JM_FORCEINLINE JM_HOT float complex
wfm_synth_step(wfm_synth_state_t *state)
{
    /* jm: body sourced from [wfm_synth] impl/impl_file in objects/wfm_synth.toml — edit there, not here; `jm apply` overwrites this. */
    float complex sym;
    if (state->shaper) {
        /* Polyphase RRC pulse shaping (power-of-two sps). The single shaping
         * kernel wfm_synth_steps() also drives, one output at a time, so step()
         * and the block path agree bit-for-bit (the resampler is block-boundary
         * invariant). Covers every shaped type — the symbol source is dispatched
         * inside wfm_synth_next_symbol(). */
        float complex s1[1];
        wfm_synth_shape(state, &sym, 1, s1);
    } else if (state->wtype == WFM_SYNTH_BITS || state->wtype == WFM_SYNTH_DSSS) {
        /* User bit pattern, oversampled sps and cycled to fill the request. The
         * symbol latch mirrors the PN path but sources bits from bits[bit_idx]
         * instead of the LFSR; bit_mod picks the mapping. A dsss burst is the
         * same machinery over the chip pattern set_dsss() assembled. */
        if (state->sym_pos == 0) {
            if (state->chips_per_symbol > 0.0) { /* continuous DSSS: lazy chip */
                state->cur_re = wfm_synth_cont_dsss_chip(state);
                state->cur_im = 0.0f;
            } else if (state->bits && state->n_bits) {
                /* ONE bits->symbol map for every order, shared with
                 * wfm_synth_next_symbol() and wfm_synth_steps(). Inlining it
                 * here is what let the QPSK copy drift into a different label
                 * assignment than mpsk_constellation() -- see
                 * wfm_synth_bit_symbol(). */
                float _Complex bs = wfm_synth_bit_symbol(state);
                state->cur_re     = crealf(bs);
                state->cur_im     = cimagf(bs);
            }
        }
        if (state->fir) {
            /* RRC pulse shaping: the same matched-FIR impulse train as the
             * PN/PSK path, sourced from the bit latch instead of the LFSR. The
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
    } else if (state->wtype == WFM_SYNTH_SYMBOLS) {
        /* User complex-symbol stream: the symbol IS the constellation point (no
         * bit->symbol mapping), oversampled sps and cycled. Generalises every
         * modulation — pi/4-QPSK, QAM, custom — into "compute symbols, pass them".
         * Shares the bits path's latch + FIR/rect machinery; only the source of
         * cur_re/cur_im differs (symbols[sym_read_idx] instead of a bit map). */
        if (state->sym_pos == 0 && state->symbols && state->n_symbols) {
            state->cur_re = crealf (state->symbols[state->sym_read_idx]);
            state->cur_im = cimagf (state->symbols[state->sym_read_idx]);
            state->sym_read_idx
                = (state->sym_read_idx + 1) % state->n_symbols;
        }
        if (state->fir) {
            float complex imp = (state->sym_pos == 0)
                                    ? (state->cur_re + state->cur_im * I)
                                    : (0.0f + 0.0f * I);
            fir_execute(state->fir, &imp, 1, &sym);
        } else {
            sym = state->cur_re + state->cur_im * I; /* rect sample-and-hold */
        }
        if (++state->sym_pos >= state->nsps)
            state->sym_pos = 0;
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
        lo_steps(state->lo, 1, &carrier, 1);
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
        awgn_generate(state->awgn, 1, &noise, 1);
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



/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition of optional fir/lo/awgn/pn children (presence-flagged) +
 * running waveform-position scalars; bits/config restored by create. */
#define WFM_SYNTH_STATE_MAGIC DP_FOURCC ('W','F','M','S')
#define WFM_SYNTH_STATE_VERSION 2u /* v2: + continuous-DSSS chip/symbol clocks */
size_t wfm_synth_state_bytes (const wfm_synth_state_t *state);
void wfm_synth_get_state (const wfm_synth_state_t *state, void *blob);
int wfm_synth_set_state (wfm_synth_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* WFM_SYNTH_CORE_H */
```


