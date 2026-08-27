/**
 * @file wfm_synth_core.h
 * @brief Synth component API.
 *
 * Lifecycle: create -> `[step / steps / reset]*` -> destroy
 *
 * Example:
 * @code
 * wfm_synth_state_t *obj = wfm_synth_create(0, 1000000.0, 0.0, 100.0, 0, 1, 8, 7, 0);
 * float complex y = wfm_synth_step(obj);
 * wfm_synth_destroy(obj);
 * @endcode
 */
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

/** Waveform type discriminant (the `type` create argument / --type choice). */
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

/** Continuous-DSSS data-symbol source (wfm_synth_set_dsss_cont's data_mode). */
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

/**
 * @brief Bits carried by one symbol of @p type — the `bps` an Eb/No needs.
 *
 * QPSK carries two, everything else one. DSSS is one because its payload is
 * BPSK, which is what makes `ebno == esno` for a DSSS source.
 */
JM_FORCEINLINE int
wfm_synth_bps (int type)
{
  return (type == WFM_SYNTH_QPSK) ? 2 : 1;
}

/**
 * @brief Convert a per-symbol or per-bit SNR to SNR over the full sample rate.
 *
 * **The one place this arithmetic lives.** A noise amplitude is always
 * referenced to fs, so every SNR mode is a conversion into that: an Es/N0
 * spreads the symbol's energy over @p span samples, and an Eb/No does the same
 * after first multiplying by the bits the symbol carries. Getting it wrong is
 * silent — the waveform is still a waveform, at an SNR nobody asked for — so
 * having it written twice is how a generator and a composer come to place
 * different noise for the same requested number.
 *
 * @param mode  RESOLVED mode: 1 fs, 2 Eb/No, 3 Es/No. Never 0 (auto) — see
 *              below.
 * @param bps   Bits per symbol, from wfm_synth_bps().
 * @param span  Samples one symbol's energy is spread over.
 * @param snr   The requested figure, in dB, in @p mode's reference.
 * @return      SNR in dB over fs, ready for awgn_amplitude_for_snr().
 *
 * **`auto` and `span` are deliberately the CALLER's**, and that is not an
 * oversight: they are the two things that legitimately differ. `wfm_synth`
 * resolves `auto` to fs for a DSSS source because at create() time it cannot
 * do better — the codes attach afterwards, so the spreading factor that sets
 * the symbol span is not yet known — while the composer resolves the same
 * source to Es/No and passes the true span (`sf * sps` for a burst, or
 * `fs/symbol_rate` for a continuous asynchronous stream, which coincide only
 * in the synchronous case that mode exists to avoid). Those differences are
 * inputs, not a second formula.
 * @code
 * // Es/No 12 dB at 8 samples/symbol -> 2.969 dB over fs
 * double fs_db = wfm_synth_snr_over_fs (3, 1, 8.0, 12.0);
 * // the same figure read as Eb/No on QPSK is 3.010 dB hotter
 * double eb_db = wfm_synth_snr_over_fs (2, wfm_synth_bps (WFM_SYNTH_QPSK),
 *                                       8.0, 12.0);
 * @endcode
 */
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

/**
 * @brief The MLS primitive polynomial table — pn's, reached by its old name.
 *
 * The table itself moved to `pn/pn_core.h` (`pn_mls_poly`), because the
 * convention it encodes is pn_create()'s tap mask and not the synth's. This
 * spelling is retained for the call sites that already use it; it forwards and
 * holds no table of its own, so the two cannot disagree.
 */
JM_FORCEINLINE uint64_t
wfm_synth_mls_poly(uint32_t n)
{
    return pn_mls_poly(n);
}
/**
 * @brief Synth state.
 *
 * Allocate with wfm_synth_create().
 */
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

/**
 * @brief Next symbol from the user bit pattern, cycled — one mapping, every M.
 *
 * **The single home for the bits->symbol map.** It had four copies: two in
 * this header (`wfm_synth_next_symbol` and `wfm_synth_step`) and two in
 * `wfm_synth_steps()`. `wfm_synth_next_symbol`'s own comment says the kernel
 * is shared "so the single-sample and block paths cannot diverge -- they call
 * the SAME function rather than each inlining the arithmetic", and the
 * arithmetic was inlined four times anyway.
 *
 * `bit_mod` is BITS PER SYMBOL, which is what its existing values already mean
 * (1 = BPSK, 2 = QPSK), so M = 1 << bit_mod and 3 = 8PSK extends the numbering
 * rather than reinterpreting it. One symbol's bits are read **MSB-first** into
 * a Gray label and handed to `mpsk_constellation()` -- the library's canonical
 * mapping, and the one `dp_ber_score()` inverts to score bit errors.
 *
 * That shared mapping is the point. The QPSK branches this replaces put `b0`
 * on the I sign and `b1` on the Q sign: the same CONSTELLATION, but two of the
 * four labels swapped against `mpsk_constellation()`. Nothing scored a QPSK
 * bit pattern against truth, so it never produced a wrong number -- but a
 * framed QPSK stream read through the canonical scorer would have shown about
 * half its symbols wrong on a perfectly working receiver, which is the
 * plausible-number failure docs/design/rx-test.md exists to stop.
 *
 * `bit_mod == 0` is not PSK -- it is the 0/1 amplitude line this type has
 * always emitted -- so it keeps its own branch.
 *
 * @param s  Synth state; `bits`/`n_bits` must be non-empty, `bit_idx` advances.
 * @return Unit-modulus constellation point (a unit-amplitude line at
 *         `bit_mod == 0`), which is what Synth's unit-power SNR reference needs.
 */
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

/**
 * @brief One continuous-DSSS chip: `code[n % n_code] ^ data`, as a BPSK sign.
 *
 * The per-chip kernel shared by `wfm_synth_step` and `wfm_synth_steps` (and the
 * manifest `impl`), so the single-sample and block paths cannot diverge — they
 * call the SAME function rather than each inlining the arithmetic. Advances the
 * code clock (`n % n_code`) and the INDEPENDENT symbol clock (`floor(n /
 * chips_per_symbol)`) off one running chip counter; at each symbol boundary it
 * refreshes the data bit from the configured source (constant 0 for code-only,
 * the cycled payload, or the next PN bit). Non-integer `chips_per_symbol` is
 * what makes symbol edges land mid-epoch — the asynchronicity.
 *
 * Requires `chips_per_symbol >= 1` (chip rate >= symbol rate, always true for a
 * real DSSS waveform), so the symbol index advances by 0 or 1 per chip and the
 * PN is never asked to skip.
 */
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

/**
 * @brief Pull the next constellation symbol from the active shaped source.
 *
 * The single symbol-generation point the polyphase pulse shaper feeds from,
 * dispatching on the waveform type exactly as `wfm_synth_step`'s symbol latch
 * does — the PN LFSR (pn/bpsk one chip, qpsk two Gray chips), the cycled user
 * bit pattern (bits, per bit_mod), the continuous asynchronous DSSS chip, or
 * the cycled complex-symbol stream — and advancing that source's read cursor by
 * one symbol. Only the shaped types (pn/bpsk/qpsk/bits/symbols/dsss, the set
 * `wfm_synth_set_rrc` accepts) reach here, so the shaper draws the *same* symbol
 * sequence the dense-FIR path would; only the pulse-shaping filter differs.
 */
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

/**
 * @brief Prime the shaper's delay line so its output aligns with the dense FIR.
 *
 * The polyphase interpolator emits its first meaningful sample only after the
 * delay line fills, so its output lags the dense-FIR path by exactly `nsps`
 * samples. Discarding that many leading outputs once, at stream start (which
 * consumes exactly the first source symbol into the delay line), realigns the
 * shaped waveform to the dense path to float precision — so switching a source
 * to polyphase shaping does not shift downstream sample timing. Idempotent via
 * the `primed` flag; re-armed by `wfm_synth_reset`.
 */
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

/**
 * @brief Produce `m` polyphase-shaped baseband samples into `out`.
 *
 * The one shaping kernel shared by `wfm_synth_step` (m == 1) and
 * `wfm_synth_steps` (m == block): prime once, generate exactly the
 * `resamp_interp_inputs_needed(shaper, m)` symbols this call consumes into the
 * caller's `syms` scratch, and fill `m` outputs. Because the resampler is
 * block-boundary invariant and both faces call this identical routine, a single
 * m-sample call and m one-sample calls produce bit-identical output — the
 * step()==steps() guarantee. Carrier mix and noise are applied by the caller.
 *
 * @param s     Shaper-attached synth state (`s->shaper != NULL`).
 * @param out   Output buffer, capacity >= @p m.
 * @param m     Number of baseband samples to produce.
 * @param syms  Caller scratch, capacity >= resamp_interp_inputs_needed(s, m).
 */
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

/**
 * @brief Allocate and configure a waveform synthesiser.
 * The synthesiser combines a local oscillator (LO), optional AWGN, and an
 * optional PN LFSR into a single streaming source.  One call to
 * wfm_synth_step() or wfm_synth_steps() advances all sub-components in lock-step.
 * SNR >= WFM_SYNTH_SNR_CLEAN (100 dB) skips AWGN entirely — clean waveforms
 * pay no noise overhead.  When ``snr_mode`` is "auto" the library picks the
 * natural reference: Es/No for modulated types (BPSK, QPSK), fs-band SNR
 * for tone/noise/PN.
 *
 * @param type  Waveform type: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk,
 *              5=chirp, 6=bits, 7=symbols, 8=dsss.  The Python binding accepts
 *              strings
 *              "tone"|"noise"|"pn"|"bpsk"|"qpsk"|"chirp"|"bits"|"symbols"|"dsss".
 *              For "bits" attach the pattern with wfm_synth_set_bits(); for
 *              "symbols" attach the complex stream with wfm_synth_set_symbols();
 *              for "dsss" attach the burst with wfm_synth_set_dsss() after
 *              create().
 * @param fs  Sample rate in Hz.  Sets the carrier frequency normalisation
 *              and the noise bandwidth.  Default 1 000 000.0.
 * @param freq  Carrier frequency offset in Hz (−fs/2 … fs/2).  A
 *              complex LO is created only when freq != 0.  For a chirp this
 *              is the start frequency f_start (the instantaneous frequency at
 *              t=0).  Default 0.0.
 * @param snr  Target SNR in dB, interpreted per ``snr_mode``.  Values >=
 *              WFM_SYNTH_SNR_CLEAN (100) disable AWGN.  Default 100.0.
 * @param snr_mode  SNR reference: 0=auto, 1=fs (full-band), 2=ebno,
 *              3=esno.  The Python binding accepts strings
 *              "auto"|"fs"|"ebno"|"esno".  Default 0.
 * @param seed  PRNG seed shared by AWGN and the PN LFSR.  Default 1.
 * @param sps  Samples per symbol for modulated types (BPSK, QPSK, PN).
 *              Ignored for tone/noise.  Default 8.
 * @param pn_length  LFSR register length (1..64); period = 2^pn_length - 1.
 *              Default 7 (period 127).
 * @param pn_poly  Galois tap polynomial for the LFSR.  0 means "look up
 *              the canonical MLS polynomial for pn_length" from the
 *              wfm_synth_mls_poly table.  Default 0.
 * @param lfsr  LFSR realization: PN_GALOIS (0) or PN_FIBONACCI (1).
 * @param f_end  Chirp end frequency in Hz (type=chirp only; ignored otherwise).
 *              With ``freq`` as the start, the instantaneous frequency sweeps
 *              linearly from ``freq`` to ``f_end`` over the span (set by
 *              wfm_synth_set_chirp_span() or the first wfm_synth_steps() call),
 *              then holds at ``f_end``.  ``f_end < freq`` is a down-chirp.
 *              Default 0.0.
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call wfm_synth_destroy() when done.
 * @code
 * >>> from doppler.wfm import _SynthEngine
 * >>> import numpy as np
 * >>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> x = s.steps(4)
 * >>> x.dtype
 * dtype('complex64')
 * >>> x.tolist()
 * [(1+0j), (1+0j), (1+0j), (1+0j)]
 * @endcode
 */
wfm_synth_state_t *wfm_synth_create(int type, double fs, double freq, double snr, int snr_mode, uint32_t seed, int sps, int pn_length, uint64_t pn_poly, int lfsr, double f_end);

/**
 * @brief Pin a chirp's sweep span to @p span samples (no-op for non-chirp).
 *
 * A linear chirp's slope is `(f_end − f_start) / span`, so the span — the
 * number of samples the sweep occupies — must be known before generation. The
 * composer/CLI call this with the segment length; a standalone synth that is
 * never pinned locks its span to the first wfm_synth_steps() call instead.
 * Only the first pin (while the span is still 0) takes effect, so it is safe to
 * call unconditionally after wfm_synth_create().
 *
 * @param state  Must be non-NULL.
 * @param span   Sweep length in samples (> 0).
 */
void wfm_synth_set_chirp_span(wfm_synth_state_t *state, size_t span);

/**
 * @brief Attach a user bit pattern to a type=bits synth (no-op otherwise).
 *
 * Copies @p n bits (each 0/1) into the synth; @p modulation maps them to
 * symbols (0=none → 0/1 amplitude, 1=bpsk → ±1, 2=qpsk → Gray-coded ±1/√2,
 * two bits per symbol). The pattern is oversampled by the create-time `sps`
 * and **cycled** to fill whatever length `wfm_synth_steps()` requests, so one
 * pass is `n * sps` samples (`2*ceil... ` — `n/2 * sps` for qpsk). Replaces any
 * previous pattern; resets the read position. Safe to call repeatedly.
 *
 * @param state  Must be non-NULL.
 * @param bits   Array of @p n bytes, each 0 or 1.
 * @param n      Number of bits (> 0).
 * @param modulation  0=none, 1=bpsk, 2=qpsk.
 * @return 0 on success; -1 on bad args or allocation failure.
 */
int wfm_synth_set_bits(wfm_synth_state_t *state, const uint8_t *bits, size_t n,
                       int modulation);

/**
 * @brief Build and attach a two-code DSSS burst to a type=dsss synth (no-op
 * otherwise).
 *
 * Assembles the burst chip pattern through `wfm_frame_dsss_chips()` — an
 * unmodulated preamble (`acq_code` repeated `acq_reps` times, the coherent
 * acquisition target) followed by the frame `sync | payload | CRC-16`, each
 * frame bit XOR-spread by the distinct `data_code` — and installs it as the
 * synth's BPSK chip stream (each chip held for the create-time `sps`
 * samples, i.e. `sps` is samples per *chip* here). This is the transmit
 * side of `BurstDemod`'s frame contract: the same codes, sync word, and
 * payload length hand to `burst_demod_set_preamble`/`set_sync` on receive.
 *
 * One pass of the pattern is one burst (`n_chips * sps` samples); like the
 * bits pattern it cycles if more samples are requested — the composer sizes
 * a dsss segment's on-time to exactly one burst. Replaces any previous
 * pattern; resets the read position.
 *
 * NOTE: `snr_mode` semantics — the raw engine's create-time esno refers to
 * the *chip* (the output symbol). The Segment/Synth faces convert a
 * data-symbol Es/N0 (`snr_mode="esno"`) to the over-fs value with
 * `10*log10(sf*sps)` before create; see `wfm_snr_over_fs()`.
 *
 * @param state        Must be non-NULL.
 * @param acq_code     Preamble code (0/1), length @p acq_len; NULL when
 *                     `acq_len*acq_reps == 0`.
 * @param acq_len      Preamble code length in chips.
 * @param acq_reps     Preamble repetitions.
 * @param data_code    Payload spreading code (0/1), length @p data_len.
 * @param data_len     Chips per frame symbol (the spreading factor).
 * @param sync         Frame-sync word bits (0/1); NULL for none.
 * @param sync_len     Sync word length in bits.
 * @param payload      Payload bits (0/1); NULL for a preamble-only burst.
 * @param payload_len  Payload length in bits.
 * @param crc          Non-zero: append a CRC-16-CCITT trailer (dp_crc16.h)
 *                     over the payload bits.
 * @return 0 on success; -1 on invalid geometry (frame bits with no data
 *         code, or an empty burst) or allocation failure.
 */
int wfm_synth_set_dsss(wfm_synth_state_t *state, const uint8_t *acq_code,
                       size_t acq_len, size_t acq_reps,
                       const uint8_t *data_code, size_t data_len,
                       const uint8_t *sync, size_t sync_len,
                       const uint8_t *payload, size_t payload_len, int crc);

/**
 * @brief Install an already-assembled DSSS burst as the chip pattern.
 *
 * The spreading half of `wfm_synth_set_dsss()`, split out so a caller who
 * assembled the frame from a `wfm_frame_desc_t` -- a burst carrying an inner
 * code, an ASM, an outer code or a randomiser -- installs it through the same
 * path as the four-field form rather than through a second one. Chips are
 * copied; @p chips stays the caller's.
 *
 * @param state    Synth (no-op unless `wtype == WFM_SYNTH_DSSS`).
 * @param chips    Burst chips, one per byte (0/1), BPSK-mapped by the synth.
 * @param n_chips  Chip count; must be non-zero.
 * @return 0 on success, -1 on a NULL/empty pattern or allocation failure.
 */
int wfm_synth_set_dsss_chips(wfm_synth_state_t *state, const uint8_t *chips,
                             size_t n_chips);


/**
 * @brief Configure a type=dsss synth for CONTINUOUS ASYNCHRONOUS generation.
 *
 * The continuous counterpart to wfm_synth_set_dsss(): the same `type="dsss"`
 * waveform, switched to the endless mode by supplying `chips_per_symbol` (=
 * `chip_rate / symbol_rate`). One waveform type, one discriminator — no tenth
 * entry in the five hand-maintained name tables `wfm_names.h` records rotting
 * once already.
 *
 * **Lazy, not materialised.** Chips are generated per sample by
 * `wfm_synth_cont_dsss_chip` off a running counter, so the stream is genuinely
 * endless — there is no pattern length to pick and the standalone `Synth` face
 * works unbounded. The data-symbol source is chosen by @p data_mode:
 *   - `WFM_DSSS_DATA_NONE` — code-only: the pure spreading code, no data.
 *   - `WFM_DSSS_DATA_BITS` — @p data, cycled mod @p n_data (caller holds it).
 *   - `WFM_DSSS_DATA_PRBS` — the synth's own seeded PN (create it in create();
 *     a receiver regenerates the bits via `doppler.wfm.PN`).
 *
 * The burst frame parameters have no meaning here (no preamble, sync, or CRC);
 * the caller rejects that combination upstream rather than ignoring it (see
 * wfmgen's `--symbol-rate` validation), so this function does not revisit it.
 *
 * @param state       Synth (no-op unless `wtype == WFM_SYNTH_DSSS`).
 * @param code        Spreading code chips (0/1), length @p code_len; copied.
 * @param code_len    Spreading code length in chips (> 0) — the SF.
 * @param chips_per_symbol  Chips per data symbol (>= 1), `chip_rate /
 *                    symbol_rate`. Non-integer is the normal, asynchronous case.
 * @param data_mode   WFM_DSSS_DATA_{NONE,BITS,PRBS}.
 * @param data        Payload bits (0/1) for WFM_DSSS_DATA_BITS, length
 *                    @p n_data; copied. Ignored (may be NULL) otherwise.
 * @param n_data      Payload length in bits (> 0 for WFM_DSSS_DATA_BITS).
 * @return 0 on success; -1 on invalid geometry or allocation failure.
 */
int wfm_synth_set_dsss_cont(wfm_synth_state_t *state, const uint8_t *code,
                            size_t code_len, double chips_per_symbol,
                            int data_mode, const uint8_t *data, size_t n_data);

/**
 * @brief Attach a complex-symbol stream to a type=symbols synth (no-op else).
 *
 * Copies @p n complex symbols into the synth. Each symbol **is** the
 * constellation point — there is no bit→symbol mapping, so this generalises
 * every modulation (pi/4-QPSK, QAM, custom shaping) into "compute the symbols,
 * pass them in". The stream is oversampled by the create-time `sps` and
 * **cycled** to fill whatever length `wfm_synth_steps()` requests (one pass is
 * `n * sps` samples), and is RRC-shaped when `wfm_synth_set_rrc()` is active.
 * Replaces any previous stream; resets the read position. Safe to call
 * repeatedly.
 *
 * @param state    Must be non-NULL.
 * @param symbols  Array of @p n complex symbols (copied).
 * @param n        Number of symbols (> 0).
 * @return 0 on success; -1 on bad args or allocation failure.
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import _SynthEngine, rrc_taps
 * >>> s = _SynthEngine(
 * ...     type="symbols", fs=1.0, freq=0.0, snr=100.0, sps=4)
 * >>> s.set_symbols(np.array([1+0j, 1j, -1+0j, -1j], np.complex64))
 * >>> s.steps(4)[::4].tolist()   # symbol centres (rect hold)
 * [(1+0j), (1+0j), (1+0j), (1+0j)]
 * @endcode
 */
int wfm_synth_set_symbols(wfm_synth_state_t *state,
                          const float _Complex *symbols, size_t n);

/**
 * @brief Enable RRC pulse shaping on a symbol synth (pn/bpsk/qpsk/bits).
 *
 * Replaces the default rectangular sample-and-hold with a root-raised-cosine
 * pulse: the symbol-rate impulse train is filtered by @p taps (a real FIR of
 * @p ntaps coefficients, typically `wfm_rrc_taps(beta, sps, span)`). The taps
 * are scaled by sqrt(sps) internally for unit transmit power, so every caller
 * passes the raw taps and gets byte-identical shaping. No-op for types with no
 * symbol stream (tone/noise/chirp). Replaces any existing shaper and clears its
 * delay line.
 *
 * @param state  Must be non-NULL.
 * @param taps   Real FIR taps (copied).
 * @param ntaps  Number of taps (> 0).
 * @return 0 on success; -1 on bad args / allocation failure.
 */
int wfm_synth_set_rrc(wfm_synth_state_t *state, const float *taps,
                      size_t ntaps);

/**
 * @brief Destroy a synth instance and release all memory.
 * Recursively frees the LO, AWGN, and PN sub-objects, then the struct
 * itself.  Safe to call with NULL (no-op).
 *
 * @param state  Pointer to heap-allocated state; may be NULL.
 * @code
 * >>> from doppler.wfm import _SynthEngine
 * >>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> s.destroy()   # explicit teardown; no exception
 * @endcode
 */
void wfm_synth_destroy(wfm_synth_state_t *state);

/**
 * @brief Reset Synth to its post-create state.
 * Resets the LO phase accumulator, AWGN internal state, and PN LFSR
 * register to their initial values so the output sequence is perfectly
 * reproducible from sample 0.
 *
 * @param state  Must be non-NULL.
 * @code
 * >>> from doppler.wfm import _SynthEngine
 * >>> import numpy as np
 * >>> s = _SynthEngine(type="qpsk", sps=4, seed=1, snr=100.0)
 * >>> a = s.steps(16).copy()
 * >>> s.reset()
 * >>> np.array_equal(a, s.steps(16))
 * True
 * @endcode
 */
void wfm_synth_reset(wfm_synth_state_t *state);

/**
 * @brief Reseed only the additive-noise (AWGN) generator, leaving the signal
 * (LO / PN code / data / pulse shaping) untouched. A no-op for a synth with no
 * noise. Used by the composer to give each repeat a fresh noise realization
 * while the underlying waveform stays bit-identical.
 * @param state  Synth state (may be NULL).
 * @param seed   New noise RNG seed.
 */
void wfm_synth_reseed_noise(wfm_synth_state_t *state, uint32_t seed);

/**
 * @brief Generate n noise-only samples — the synth's additive-AWGN term with
 * no signal — continuing the same noise RNG stream wfm_synth_steps() draws
 * from (no reseed, identical chunked awgn call pattern, so a gap rendered
 * here is the seamless continuation of the on-time noise). Writes exact
 * zeros and advances nothing for a clean synth (no AWGN child). Used by the
 * composer to carry a segment's noise floor through its off-time gap.
 * @param state   Synth state (may be NULL — no-op).
 * @param output  n complex samples out.
 * @param n       Sample count.
 */
void wfm_synth_noise_steps(wfm_synth_state_t *state, float complex *output,
                           size_t n);

/**
 * @brief Generate one output sample from internal state.
 * Advances the PN LFSR (modulated types only, on symbol boundaries), the
 * LO phase accumulator, and the AWGN engine, then returns the mixed
 * result: ``sym * carrier + noise``.  Inlined and hot-path annotated so
 * tight per-sample loops pay no call overhead.
 *
 * @param state  Must be non-NULL.
 * @return Next output sample (float complex).
 * @code
 * >>> from doppler.wfm import _SynthEngine
 * >>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> s.step()
 * (1+0j)
 * @endcode
 */
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

/**
 * @brief Generate a block of output samples.
 * Calls wfm_synth_step() in a tight loop, writing each cf32 sample into
 * ``output``.  The Python binding returns a freshly allocated NumPy
 * complex64 array; ownership is transferred to the caller.
 *
 * @param state   Initialised Synth state returned by ``wfm_synth_create``.
 * @param output  Output buffer of at least ``n`` cf32 elements.
 * @param n       Number of samples to generate.
 * @code
 * >>> from doppler.wfm import _SynthEngine
 * >>> import numpy as np
 * >>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> x = s.steps(4)
 * >>> x.shape, x.dtype
 * ((4,), dtype('complex64'))
 * >>> x.tolist()
 * [(1+0j), (1+0j), (1+0j), (1+0j)]
 * @endcode
 */
void wfm_synth_steps(
    wfm_synth_state_t *state,
    float complex          *output,
    size_t               n);

/**
 * @brief Return the active waveform type discriminant.
 * Maps to the WFM_SYNTH_* enum: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk.
 * Use this to inspect which synthesis path is active at runtime.
 *
 * @param state  Must be non-NULL.
 * @return Integer waveform type index (WFM_SYNTH_TONE .. WFM_SYNTH_QPSK).
 */
int wfm_synth_get_wtype(const wfm_synth_state_t *state);

/**
 * @brief Override the waveform type discriminant in-place.
 * Changing wtype does not reinitialise sub-objects; use with care.
 *
 * @param state  Must be non-NULL.
 * @param val    New wtype value (WFM_SYNTH_TONE .. WFM_SYNTH_QPSK).
 */
void wfm_synth_set_wtype(wfm_synth_state_t *state, int val);

/**
 * @brief Return the samples-per-symbol count.
 * For modulated types (BPSK, QPSK, PN) each symbol is held for nsps
 * consecutive output samples.  For tone/noise this field is present but
 * unused by the synthesis path.
 *
 * @param state  Must be non-NULL.
 * @return Samples per symbol (nsps >= 1).
 */
int wfm_synth_get_nsps(const wfm_synth_state_t *state);

/**
 * @brief Override the samples-per-symbol count in-place.
 * Does not flush the symbol-position counter (sym_pos); set sym_pos=0
 * as well when changing sps mid-stream.
 *
 * @param state  Must be non-NULL.
 * @param val    New nsps value (>= 1).
 */
void wfm_synth_set_nsps(wfm_synth_state_t *state, int val);

/**
 * @brief Return the current position within the current symbol (0..nsps-1).
 * Reaches nsps and wraps to 0 each time a new symbol is consumed from the
 * PN LFSR.  Useful for frame alignment: sym_pos==0 on a step boundary
 * means the very next sample begins a fresh symbol.
 *
 * @param state  Must be non-NULL.
 * @return Symbol position counter (0 <= sym_pos < nsps).
 */
int wfm_synth_get_sym_pos(const wfm_synth_state_t *state);

/**
 * @brief Override the symbol-position counter in-place.
 * Injecting 0 forces the next wfm_synth_step() to latch a new PN chip; any
 * other value fast-forwards into the middle of the current symbol hold.
 *
 * @param state  Must be non-NULL.
 * @param val    New sym_pos value (0 <= val < nsps).
 */
void wfm_synth_set_sym_pos(wfm_synth_state_t *state, int val);

/**
 * @brief Return the real part of the current held symbol.
 * For modulated types this is the I component latched at the last symbol
 * boundary (±1 for BPSK/PN, ±1/√2 for QPSK).  For tone the synthesiser
 * initialises cur_re to 1.0 so that the held symbol is a clean unit-power
 * carrier; for noise it is 0.0 (noise has no held symbol).
 *
 * @param state  Must be non-NULL.
 * @return Current symbol real (I) component.
 */
float wfm_synth_get_cur_re(const wfm_synth_state_t *state);

/**
 * @brief Override the held-symbol real (I) component in-place.
 * Takes effect on the next wfm_synth_step() within the current symbol hold.
 *
 * @param state  Must be non-NULL.
 * @param val    New cur_re value.
 */
void wfm_synth_set_cur_re(wfm_synth_state_t *state, float val);

/**
 * @brief Return the imaginary part of the current held symbol.
 * For QPSK this is the Q component (±1/√2); for BPSK/PN it is always 0;
 * for tone/noise it is 0.
 *
 * @param state  Must be non-NULL.
 * @return Current symbol imaginary (Q) component.
 */
float wfm_synth_get_cur_im(const wfm_synth_state_t *state);

/**
 * @brief Override the held-symbol imaginary (Q) component in-place.
 * Takes effect on the next wfm_synth_step() within the current symbol hold.
 *
 * @param state  Must be non-NULL.
 * @param val    New cur_im value.
 */
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
