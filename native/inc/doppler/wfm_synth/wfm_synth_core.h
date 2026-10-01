/**
 * @file wfm_synth_core.h
 * @brief Synth component API.
 *
 * Lifecycle: create -> `[step / steps / reset]*` -> destroy
 *
 * Example:
 * @code
 * dp_wfm_synth_state_t *obj = dp_wfm_synth_create(0, 1000000.0, 0.0, 100.0, 0, 1, 8, 7, 0);
 * float _Complex y = dp_wfm_synth_step(obj);
 * dp_wfm_synth_destroy(obj);
 * @endcode
 */
#ifndef DP_WFM_SYNTH_CORE_H
#define DP_WFM_SYNTH_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include "doppler/fir/fir_core.h"
#include "doppler/lo/lo_core.h"
#include "doppler/awgn/awgn_core.h"
#include "doppler/pn/pn_core.h"
#include "doppler/resamp/resamp_core.h"
#include <math.h> /* log10/powf/sqrtf in create_impl */
#include "doppler/gold/gold_core.h"
#include "doppler/wfm/wfm_dsp.h" /* dp_wfm_dsss_cont_edge: the symbol clock */
#include "doppler/mpsk/mpsk_core.h" /* mpsk_constellation — the ONE bit->symbol map */
#include "doppler/cvt/cvt_core.h"
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
    WFM_SYNTH_BITS = 6,  /* user bit pattern, oversampled, sent once  */
    WFM_SYNTH_SYMBOLS
    = 7,                /* user complex-symbol stream, oversampled + cycled */
    WFM_SYNTH_DSSS = 8, /* two-code DSSS burst: repeated preamble +
                           spread frame, installed by
                           dp_wfm_synth_set_dsss_chips();
                           OR a continuous asynchronous stream when a
                           symbol_rate is supplied (dp_wfm_synth_set_dsss_cont).
                           The two modes share this one type — a symbol_rate
                           discriminates, no tenth waveform type. */
};

/** Continuous-DSSS data-symbol source (dp_wfm_synth_set_dsss_cont's data_mode). */
enum {
    WFM_DSSS_DATA_NONE = 0, /* code-only: constant bit 0 -> the pure code    */
    WFM_DSSS_DATA_BITS = 1, /* a payload sent once, or a data source's bits */
    WFM_DSSS_DATA_PRBS = 2, /* bits from the seeded PN LFSR (regenerable)     */
};

/** `cur_data` once a continuous stream's data has ended: not a bit, so the
 *  chips are silent from then on, and it is a byte of the serialized state
 *  like the bit it replaces. */
#define WFM_DSSS_ENDED 2u

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
 * @return      SNR in dB over fs, ready for dp_awgn_amplitude_for_snr().
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
 * convention it encodes is dp_pn_create()'s tap mask and not the synth's. This
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
 * Allocate with dp_wfm_synth_create().
 */
/**
 * @brief Where a BITS synth's next frame (or a dsss burst's next burst)
 *        comes from, in place of the cycle.
 *
 * Called when the synth has read the last of its `n` bits: write the next
 * frame's `n` bits into @p bits and return 0, or return non-zero when the
 * data has ended. The frame length never changes, which is what lets the
 * synth refill its own buffer in place. See dp_wfm_synth_set_refill().
 */
typedef int (*wfm_synth_refill_fn)(void *user, uint8_t *bits, size_t n);

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
    size_t code_only_symbols; /* config: pure-code symbols opening each frame  */
    size_t frame_symbols;     /* config: frame length in symbols; 0 = no window */
    uint64_t chip_n;         /* running: chips emitted so far                 */
    uint64_t sym_idx;        /* running: current data-symbol index            */
    uint64_t next_edge;      /* derived: first chip of symbol sym_idx + 1
                                (dp_wfm_dsss_cont_edge); not serialized --
                                set_state recomputes it from sym_idx       */
    uint8_t cur_data;        /* running: data bit latched for this symbol      */
    dp_fir_state_t * fir;       /* dense RRC FIR (non-power-of-two sps fallback)  */
    /* Polyphase RRC pulse shaper: a resamp interpolate-by-sps view over the
       RRC bank, replacing the dense fir (impulse-train + full FIR) with ~sps×
       fewer MACs. Built by dp_wfm_synth_set_rrc when sps is a power of two; the
       dense `fir` is used otherwise. Exactly one of fir/shaper is ever set. */
    resamp_state_t * shaper;
    uint8_t primed;          /* running: shaper's sps-sample latency primed     */
    dp_lo_state_t * lo;
    dp_awgn_state_t * awgn;
    dp_pn_state_t * pn;
    /* A frame source (dp_wfm_synth_set_refill): NULL sends `bits` once.
       Once it reports the end, `data_ended` latches and every symbol after is zero --
       silence, never a held symbol (a line on the air nobody sent). */
    wfm_synth_refill_fn refill;
    void *refill_user;
    void (*refill_free)(void *);
    uint8_t data_ended;
} dp_wfm_synth_state_t;

/**
 * @brief The next bit of the pattern, or none once the data has ended.
 *
 * The cursor STOPS at `n_bits`; it never wraps (doppler#1718: the cycle
 * that sent one pattern again and again is gone). With a refill, that one
 * bounds check finds the frame boundary: the next frame is drawn lazily,
 * when its first bit is due -- the source's counts are frames started, and
 * a paced source is asked when the frame is due. A refill that reports the
 * end latches `data_ended`, and so does the end of a pattern with no
 * refill: it was the whole of the data, sent once. Either way every later
 * call is the slow path's immediate "no bit", and the caller sends silence -- never a line nobody
 * sent. The one place the cursor moves, so the per-sample and block paths
 * cannot disagree about a frame boundary.
 *
 * @param s    the synth; a type=bits synth with a pattern set.
 * @param bit  receives the next bit, 0 or 1, when one is returned.
 * @return 1 with the bit in @p *bit, or 0: the data has ended.
 */
JM_FORCEINLINE int
wfm_synth_bit_next(dp_wfm_synth_state_t *s, unsigned *bit)
{
    if (s->bit_idx >= s->n_bits) { /* the pattern is spent */
        if (s->data_ended)
            return 0;
        /* No source to refill from: the pattern was the whole of the data,
           sent once -- the same end as a source that reports one. */
        if (!s->refill || s->refill(s->refill_user, s->bits, s->n_bits) != 0) {
            s->data_ended = 1;
            return 0;
        }
        s->bit_idx = 0;
    }
    *bit = s->bits[s->bit_idx++] ? 1u : 0u;
    return 1;
}

/**
 * @brief Next symbol from the user bit pattern — one mapping, every M.
 *
 * **The single home for the bits->symbol map.** It had four copies: two in
 * this header (`wfm_synth_next_symbol` and `dp_wfm_synth_step`) and two in
 * `dp_wfm_synth_steps()`. `wfm_synth_next_symbol`'s own comment says the kernel
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
wfm_synth_bit_symbol(dp_wfm_synth_state_t *s)
{
    unsigned g = 0u, b = 0u;
    int      k;
    if (s->bit_mod <= 0) {
        if (!wfm_synth_bit_next(s, &b))
            return 0.0f + 0.0f * I; /* silence after the data */
        return (b ? 1.0f : 0.0f) + 0.0f * I;
    }
    for (k = 0; k < s->bit_mod; k++) { /* MSB-first within the symbol */
        if (!wfm_synth_bit_next(s, &b)) {
            /* Silence once the data has ended -- never a held symbol, a
               line on the air nobody sent. A symbol the end cuts short is
               completed with zero bits: the frame did not divide into
               symbols, and the zeros only finish the symbol it started. */
            if (k == 0)
                return 0.0f + 0.0f * I;
            b = 0u;
        }
        g = (g << 1) | b;
    }
    return mpsk_constellation(g, 1 << s->bit_mod);
}

/**
 * @brief One continuous-DSSS chip: `code[n % n_code] ^ data`, as a BPSK sign.
 *
 * The per-chip kernel shared by `dp_wfm_synth_step` and `dp_wfm_synth_steps` (and the
 * manifest `impl`), so the single-sample and block paths cannot diverge — they
 * call the SAME function rather than each inlining the arithmetic. Advances the
 * code clock (`n % n_code`) and the INDEPENDENT symbol clock (`floor(n /
 * chips_per_symbol)`) off one running chip counter; at each symbol boundary it
 * refreshes the data bit from the configured source (constant 0 for code-only,
 * the next payload bit, or the next PN bit). Non-integer `chips_per_symbol` is
 * what makes symbol edges land mid-epoch — the asynchronicity.
 *
 * With a frame set (`dp_wfm_synth_set_dsss_window`), the frame lives on the
 * SYMBOL clock: of every `frame_symbols` symbols, the first
 * `code_only_symbols` carry data 0 — the pure code — and the rest carry the
 * payload, whose index counts data symbols only, so the bits run on across
 * frames. The symbol clock never restarts: it is the same free-running
 * `floor(n / chips_per_symbol)` with or without a window, so a frame edge
 * falls at whatever chip phase that clock puts it — the chip and data clocks
 * have no fixed relation, and no frame edge is synchronous with a code epoch.
 * `frame_symbols == 0` is the windowless stream, bit for bit.
 *
 * Requires `chips_per_symbol >= 1` (chip rate >= symbol rate, always true for a
 * real DSSS waveform), so the symbol index advances by 0 or 1 per chip and the
 * PN is never asked to skip.
 */
JM_FORCEINLINE float
wfm_synth_cont_dsss_chip(dp_wfm_synth_state_t *s)
{
    uint64_t n = s->chip_n;
    /* The symbol clock is dp_wfm_dsss_cont_edge's, the one every caller
       shares: a symbol opens on its edge chip, and cps >= 1 means at most
       one edge per chip. One call per symbol, not a division per chip. */
    if (n == 0 || n >= s->next_edge) {
        const uint64_t sym = n ? s->sym_idx + 1u : 0u;
        s->sym_idx         = sym;
        s->next_edge = dp_wfm_dsss_cont_edge(sym + 1u, s->chips_per_symbol);
        const uint64_t F = s->frame_symbols, W = s->code_only_symbols;
        if (n && s->cur_data == WFM_DSSS_ENDED) {
            /* the data has ended: silent for good, window or not */
        } else if (F && sym % F < W) {
            s->cur_data = 0u; /* the pure-code window: the code, +polarity */
        } else if (s->data_mode == WFM_DSSS_DATA_PRBS) {
            s->cur_data = s->pn ? pn_step(s->pn) : 0u; /* data symbols only */
        } else if (s->data_mode == WFM_DSSS_DATA_BITS) {
            /* the next bit through the one cursor, one per data symbol --
               the code-only window takes none -- from a data source
               (doppler#1719) or a payload sent once (doppler#1718); once
               there is none, WFM_DSSS_ENDED: silence, for good */
            unsigned b  = 0u;
            s->cur_data = wfm_synth_bit_next(s, &b) ? (uint8_t)b
                                                    : WFM_DSSS_ENDED;
        } else {
            s->cur_data = 0u; /* code-only: the pure code, +code polarity */
        }
    }
    uint8_t code_bit = (uint8_t)(s->code[n % s->n_code] & 1u);
    s->chip_n        = n + 1;
    if (s->cur_data == WFM_DSSS_ENDED)
        return 0.0f; /* silence after the data's last bit */
    return (code_bit ^ s->cur_data) ? -1.0f : 1.0f;
}

/**
 * @brief Pull the next constellation symbol from the active shaped source.
 *
 * The single symbol-generation point the polyphase pulse shaper feeds from,
 * dispatching on the waveform type exactly as `dp_wfm_synth_step`'s symbol latch
 * does — the PN LFSR (pn/bpsk one chip, qpsk two Gray chips), the user bit
 * pattern (bits, per bit_mod, sent once), the continuous asynchronous DSSS chip, or
 * the cycled complex-symbol stream — and advancing that source's read cursor by
 * one symbol. Only the shaped types (pn/bpsk/qpsk/bits/symbols/dsss, the set
 * `dp_wfm_synth_set_rrc` accepts) reach here, so the shaper draws the *same* symbol
 * sequence the dense-FIR path would; only the pulse-shaping filter differs.
 */
JM_FORCEINLINE float _Complex
wfm_synth_next_symbol(dp_wfm_synth_state_t *s)
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
 * the `primed` flag; re-armed by `dp_wfm_synth_reset`.
 */
JM_FORCEINLINE void
wfm_synth_shaper_prime(dp_wfm_synth_state_t *s)
{
    size_t left = (size_t)s->nsps;
    while (left) {
        float _Complex syms[64], scratch[64];
        size_t pm = left < 64 ? left : 64;
        size_t need = dp_resamp_interp_inputs_needed(s->shaper, pm);
        for (size_t k = 0; k < need; k++)
            syms[k] = wfm_synth_next_symbol(s);
        dp_resamp_interp_fill(s->shaper, syms, scratch, pm);
        left -= pm;
    }
    s->primed = 1;
}

/**
 * @brief Produce `m` polyphase-shaped baseband samples into `out`.
 *
 * The one shaping kernel shared by `dp_wfm_synth_step` (m == 1) and
 * `dp_wfm_synth_steps` (m == block): prime once, generate exactly the
 * `dp_resamp_interp_inputs_needed(shaper, m)` symbols this call consumes into the
 * caller's `syms` scratch, and fill `m` outputs. Because the resampler is
 * block-boundary invariant and both faces call this identical routine, a single
 * m-sample call and m one-sample calls produce bit-identical output — the
 * step()==steps() guarantee. Carrier mix and noise are applied by the caller.
 *
 * @param s     Shaper-attached synth state (`s->shaper != NULL`).
 * @param out   Output buffer, capacity >= @p m.
 * @param m     Number of baseband samples to produce.
 * @param syms  Caller scratch, capacity >= dp_resamp_interp_inputs_needed(s, m).
 */
JM_FORCEINLINE void
wfm_synth_shape(dp_wfm_synth_state_t *s, float _Complex *out, size_t m,
                float _Complex *syms)
{
    if (!s->primed)
        wfm_synth_shaper_prime(s);
    size_t need = dp_resamp_interp_inputs_needed(s->shaper, m);
    for (size_t k = 0; k < need; k++)
        syms[k] = wfm_synth_next_symbol(s);
    dp_resamp_interp_fill(s->shaper, syms, out, m);
}

/**
 * @brief Allocate and configure a waveform synthesiser.
 * The synthesiser combines a local oscillator (LO), optional AWGN, and an
 * optional PN LFSR into a single streaming source.  One call to
 * dp_wfm_synth_step() or dp_wfm_synth_steps() advances all sub-components in lock-step.
 * SNR >= WFM_SYNTH_SNR_CLEAN (100 dB) skips AWGN entirely — clean waveforms
 * pay no noise overhead.  When ``snr_mode`` is "auto" the library picks the
 * natural reference: Es/No for modulated types (BPSK, QPSK), fs-band SNR
 * for tone/noise/PN.
 *
 * @param type  Waveform type: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk,
 *              5=chirp, 6=bits, 7=symbols, 8=dsss.  The Python binding accepts
 *              strings
 *              "tone"|"noise"|"pn"|"bpsk"|"qpsk"|"chirp"|"bits"|"symbols"|"dsss".
 *              For "bits" attach the pattern with dp_wfm_synth_set_bits(); for
 *              "symbols" attach the complex stream with dp_wfm_synth_set_symbols();
 *              for "dsss" attach the burst with dp_wfm_synth_set_dsss_chips()
 *              after create().
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
 *              linearly from ``freq`` to ``f_end`` over the span set by
 *              dp_wfm_synth_set_chirp_span(), then holds at ``f_end``.  Until a
 *              span is pinned the slope is 0 (a CW tone at ``freq``).
 *              ``f_end < freq`` is a down-chirp.
 *              Default 0.0.
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call dp_wfm_synth_destroy() when done.
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
dp_wfm_synth_state_t *dp_wfm_synth_create(int type, double fs, double freq, double snr, int snr_mode, uint32_t seed, int sps, int pn_length, uint64_t pn_poly, int lfsr, double f_end);

/**
 * @brief Pin a chirp's sweep span to @p span samples (no-op for non-chirp).
 *
 * A linear chirp's slope is `(f_end − f_start) / span`, so the span — the
 * number of samples the sweep occupies — must be known before generation. The
 * composer calls this with the source's declared span or the segment length.
 * A synth that is never pinned does not sweep: it holds the start frequency on
 * dp_wfm_synth_step() and dp_wfm_synth_steps() alike, so the waveform never depends
 * on how reads are chunked.  Only the first pin (while the span is still 0)
 * takes effect, so it is safe to call unconditionally after
 * dp_wfm_synth_create(); @p span 0 is a no-op.
 *
 * The span is configuration, not running state: dp_wfm_synth_get_state() does not
 * carry it, so pin a resumed instance exactly as the original was pinned.
 *
 * @param state  Must be non-NULL.
 * @param span   Sweep length in samples (> 0).
 */
void dp_wfm_synth_set_chirp_span(dp_wfm_synth_state_t *state, size_t span);

/**
 * @brief Attach a user bit pattern to a type=bits synth (no-op otherwise).
 *
 * Copies @p n bits (each 0/1) into the synth; @p modulation maps them to
 * symbols (0=none → 0/1 amplitude, 1=bpsk → ±1, 2=qpsk → Gray-coded ±1/√2,
 * two bits per symbol). The pattern is oversampled by the create-time `sps`
 * and sent ONCE: one pass is `n * sps` samples (`n/2 * sps` for qpsk), and the
 * output is silent after it (doppler#1718 deleted the cycle; a payload that
 * goes on is a data source, dp_wfm_synth_set_refill). Replaces any
 * previous pattern; resets the read position. Safe to call repeatedly.
 *
 * @param state  Must be non-NULL.
 * @param bits   Array of @p n bytes, each 0 or 1.
 * @param n      Number of bits (> 0).
 * @param modulation  0=none, 1=bpsk, 2=qpsk.
 * @return 0 on success; -1 on bad args or allocation failure.
 */
int dp_wfm_synth_set_bits(dp_wfm_synth_state_t *state, const uint8_t *bits, size_t n,
                       int modulation);

/**
 * @brief Pull each frame from @p fn instead of cycling the pattern.
 *
 * The pattern set by dp_wfm_synth_set_bits() is the FIRST frame; when its
 * last bit has been read, @p fn writes the next frame's bits into the synth's
 * buffer (the same `n`), and so on. When @p fn reports the end, the synth
 * latches it (dp_wfm_synth_data_ended()) and emits zero -- silence -- from
 * then on. A later dp_wfm_synth_set_bits() detaches the refill.
 *
 * A dsss BURST plays its chips through the same cursor, so it takes a refill
 * too: the pattern is the burst dp_wfm_synth_set_dsss_chips() installed, and
 * @p fn writes the next burst's chips (doppler#1719). A later
 * dp_wfm_synth_set_dsss_chips() detaches it. A CONTINUOUS dsss stream set
 * up with `WFM_DSSS_DATA_BITS` takes one as well: each data symbol reads the
 * next bit through the cursor instead of cycling its payload, the refill
 * writing the next `n` bits, and the chips are silent once it reports the
 * end.
 *
 * A synth with a refill attached REFUSES serialization --
 * dp_wfm_synth_state_bytes() returns 0 and dp_wfm_synth_set_state() is
 * DP_ERR_INVALID -- because the pulled frame and the source's position are
 * state it cannot yet carry (doppler#1681).
 *
 * @param state      a type=bits synth with a pattern set, a type=dsss
 *                   burst with its chips set, or a continuous one whose
 *                   data_mode is `WFM_DSSS_DATA_BITS`.
 * @param fn         the frame source; NULL detaches.
 * @param user       passed to @p fn; owned by the synth when @p free_user
 *                   is given, which it calls on detach or destroy.
 * @param free_user  frees @p user, or NULL.
 * @return 0, or -1 if the synth is none of those, or has no pattern.
 *
 * @code
 * #include "doppler/wfm_synth/wfm_synth_core.h"
 * #include <complex.h>
 *
 * // One more 4-bit frame of ones after the first, then the end.
 * static int
 * one_more (void *u, uint8_t *bits, size_t n)
 * {
 *   int *left = u;
 *   if ((*left)-- <= 0)
 *     return 1; // the end
 *   for (size_t i = 0; i < n; i++)
 *     bits[i] = 1;
 *   return 0;
 * }
 *
 * int
 * main (void)
 * {
 *   dp_wfm_synth_state_t *s = dp_wfm_synth_create (
 *       WFM_SYNTH_BITS, 1e6, 0.0, 200.0, 0, 1, 1, 7, 0, 0, 0.0);
 *   int left = 1;
 *   dp_wfm_synth_set_bits (s, (const uint8_t[]){ 0, 1, 0, 1 }, 4, 0);
 *   dp_wfm_synth_set_refill (s, one_more, &left, NULL);
 *   float _Complex x[12];
 *   dp_wfm_synth_steps (s, x, 12); // 0101, then 1111, then silence
 *   const int ok = crealf (x[1]) > 0.5f && crealf (x[4]) > 0.5f
 *                  && cabsf (x[11]) < 1e-3f && dp_wfm_synth_data_ended (s);
 *   dp_wfm_synth_destroy (s);
 *   return ok ? 0 : 1;
 * }
 * @endcode
 */
int dp_wfm_synth_set_refill(dp_wfm_synth_state_t *state, wfm_synth_refill_fn fn,
                         void *user, void (*free_user)(void *));

/** @brief Non-zero once an attached frame source has reported its end. */
int dp_wfm_synth_data_ended(const dp_wfm_synth_state_t *state);

/**
 * @brief Install an assembled two-code DSSS burst as the chip pattern.
 *
 * The burst is assembled from a frame DESCRIPTION by
 * `dp_wfm_dsss_desc_chips()` (`wfm/wfm_frame.h`) -- an unmodulated preamble
 * (`acq_code` repeated `acq_reps` times, the coherent acquisition target)
 * followed by every bit of the assembled frame XOR-spread by the distinct
 * `data_code` -- and installed here as the synth's BPSK chip stream, each
 * chip held for the create-time `sps` samples, i.e. `sps` is samples per
 * *chip* here. The common frame `sync | payload | CRC-16` is
 * `dp_wfm_frame_fixed()`; a coded burst is any other description. This is
 * the transmit side of `BurstDemod`'s frame contract: the same codes, sync
 * word, and payload length hand to `dp_burst_demod_set_preamble`/`set_sync`
 * on receive.
 *
 * One pass of the pattern is one burst (`n_chips * sps` samples), sent once
 * like the bits pattern, and silence after it -- the composer sizes a dsss
 * segment's on-time to exactly one burst, or one per frame of a data source
 * (dp_wfm_synth_set_refill). Replaces any previous
 * pattern; resets the read position. Chips are copied; @p chips stays the
 * caller's.
 *
 * NOTE: `snr_mode` semantics — the raw engine's create-time esno refers to
 * the *chip* (the output symbol). The Segment/Synth faces convert a
 * data-symbol Es/N0 (`snr_mode="esno"`) to the over-fs value with
 * `10*log10(sf*sps)` before create; see `dp_wfm_snr_over_fs()`.
 *
 * @param state    Synth (no-op unless `wtype == WFM_SYNTH_DSSS`).
 * @param chips    Burst chips, one per byte (0/1), BPSK-mapped by the synth.
 * @param n_chips  Chip count; must be non-zero.
 * @return 0 on success, -1 on a NULL/empty pattern or allocation failure.
 */
int dp_wfm_synth_set_dsss_chips(dp_wfm_synth_state_t *state, const uint8_t *chips,
                             size_t n_chips);


/**
 * @brief Configure a type=dsss synth for CONTINUOUS ASYNCHRONOUS generation.
 *
 * The continuous counterpart to dp_wfm_synth_set_dsss_chips(): the same `type="dsss"`
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
 *   - `WFM_DSSS_DATA_BITS` — @p data, one bit per data symbol, sent ONCE:
 *     the chips are silent after its last bit (doppler#1718).
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
int dp_wfm_synth_set_dsss_cont(dp_wfm_synth_state_t *state, const uint8_t *code,
                            size_t code_len, double chips_per_symbol,
                            int data_mode, const uint8_t *data, size_t n_data);

/**
 * @brief Give the continuous DSSS stream a frame with a pure-code window.
 *
 * The frame is on the DATA clock: of every @p frame_symbols symbols, the
 * first @p code_only_symbols carry the pure spreading code and no data, and
 * the rest carry the payload, running on from the previous frame. The symbol
 * clock is the stream's own free-running one (see wfm_synth_cont_dsss_chip),
 * so a frame edge lands at whatever chip phase it lands at: the chip and data
 * clocks have no fixed relation, and no frame edge is synchronous with a code
 * epoch. This is the multi-emitter waveform's frame — 450 code-only symbols
 * then 4500 of data in the application it was written for — and the
 * searcher's coherent depth is what the window makes possible.
 * Configuration, not running state: it is kept by reset() and is not
 * serialized. The order against dp_wfm_synth_set_dsss_cont() does not matter.
 *
 * @param state              Synth (no-op unless `wtype == WFM_SYNTH_DSSS`).
 * @param code_only_symbols  Pure-code symbols opening each frame, at most
 *                           @p frame_symbols. Equal to it means code only,
 *                           for ever.
 * @param frame_symbols      Frame length in symbols; **0 means no window** —
 *                           the stream exactly as without this call.
 * @return 0 on success (and for a non-dsss synth); -1 if
 *         @p code_only_symbols exceeds a non-zero @p frame_symbols.
 */
int dp_wfm_synth_set_dsss_window(dp_wfm_synth_state_t *state,
                              size_t code_only_symbols, size_t frame_symbols);

/**
 * @brief Attach a complex-symbol stream to a type=symbols synth (no-op else).
 *
 * Copies @p n complex symbols into the synth. Each symbol **is** the
 * constellation point — there is no bit→symbol mapping, so this generalises
 * every modulation (pi/4-QPSK, QAM, custom shaping) into "compute the symbols,
 * pass them in". The stream is oversampled by the create-time `sps` and
 * **cycled** to fill whatever length `dp_wfm_synth_steps()` requests (one pass is
 * `n * sps` samples), and is RRC-shaped when `dp_wfm_synth_set_rrc()` is active.
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
int dp_wfm_synth_set_symbols(dp_wfm_synth_state_t *state,
                          const float _Complex *symbols, size_t n);

/**
 * @brief Enable RRC pulse shaping on a symbol synth (pn/bpsk/qpsk/bits).
 *
 * Replaces the default rectangular sample-and-hold with a root-raised-cosine
 * pulse: the symbol-rate impulse train is filtered by @p taps (a real FIR of
 * @p ntaps coefficients, typically `dp_wfm_rrc_taps(beta, sps, span)`). The taps
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
int dp_wfm_synth_set_rrc(dp_wfm_synth_state_t *state, const float *taps,
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
void dp_wfm_synth_destroy(dp_wfm_synth_state_t *state);

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
void dp_wfm_synth_reset(dp_wfm_synth_state_t *state);

/**
 * @brief Reseed only the additive-noise (AWGN) generator, leaving the signal
 * (LO / PN code / data / pulse shaping) untouched. A no-op for a synth with no
 * noise. Used by the composer to give each repeat a fresh noise realization
 * while the underlying waveform stays bit-identical.
 * @param state  Synth state (may be NULL).
 * @param seed   New noise RNG seed.
 */
void dp_wfm_synth_reseed_noise(dp_wfm_synth_state_t *state, uint32_t seed);

/**
 * @brief Generate n noise-only samples — the synth's additive-AWGN term with
 * no signal — continuing the same noise RNG stream dp_wfm_synth_steps() draws
 * from (no reseed, identical chunked awgn call pattern, so a gap rendered
 * here is the seamless continuation of the on-time noise). Writes exact
 * zeros and advances nothing for a clean synth (no AWGN child). Used by the
 * composer to carry a segment's noise floor through its off-time gap.
 * @param state   Synth state (may be NULL — no-op).
 * @param output  n complex samples out.
 * @param n       Sample count.
 */
void dp_wfm_synth_noise_steps(dp_wfm_synth_state_t *state, float _Complex *output,
                           size_t n);

/**
 * @brief Generate one output sample from internal state.
 * Advances the PN LFSR (modulated types only, on symbol boundaries), the
 * LO phase accumulator, and the AWGN engine, then returns the mixed
 * result: ``sym * carrier + noise``.  Inlined and hot-path annotated so
 * tight per-sample loops pay no call overhead.
 *
 * @param state  Must be non-NULL.
 * @return Next output sample (float _Complex).
 * @code
 * >>> from doppler.wfm import _SynthEngine
 * >>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> s.step()
 * (1+0j)
 * @endcode
 */
JM_FORCEINLINE JM_HOT float _Complex
dp_wfm_synth_step(dp_wfm_synth_state_t *state)
{
    /* jm: body sourced from [wfm_synth] impl/impl_file in
     * objects/wfm_synth.toml — edit there, not here; `jm apply` overwrites
     * this. */
    float _Complex sym;
    if (state->shaper) {
        /* Polyphase RRC pulse shaping (power-of-two sps). The single shaping
         * kernel dp_wfm_synth_steps() also drives, one output at a time, so step()
         * and the block path agree bit-for-bit (the resampler is block-boundary
         * invariant). Covers every shaped type — the symbol source is dispatched
         * inside wfm_synth_next_symbol(). */
        float _Complex s1[1];
        wfm_synth_shape(state, &sym, 1, s1);
    } else if (state->wtype == WFM_SYNTH_BITS || state->wtype == WFM_SYNTH_DSSS) {
        /* User bit pattern, oversampled sps and cycled to fill the request. The
         * symbol latch mirrors the PN path but sources bits from bits[bit_idx]
         * instead of the LFSR; bit_mod picks the mapping. A dsss burst is the
         * same machinery over the chip pattern set_dsss_chips() installed. */
        if (state->sym_pos == 0) {
            if (state->chips_per_symbol > 0.0) { /* continuous DSSS: lazy chip */
                state->cur_re = wfm_synth_cont_dsss_chip(state);
                state->cur_im = 0.0f;
            } else if (state->bits && state->n_bits) {
                /* ONE bits->symbol map for every order, shared with
                 * wfm_synth_next_symbol() and dp_wfm_synth_steps(). Inlining it
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
            float _Complex imp = (state->sym_pos == 0)
                                    ? (state->cur_re + state->cur_im * I)
                                    : (0.0f + 0.0f * I);
            dp_fir_execute(state->fir, &imp, 1, &sym);
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
            float _Complex imp = (state->sym_pos == 0)
                                    ? (state->cur_re + state->cur_im * I)
                                    : (0.0f + 0.0f * I);
            dp_fir_execute(state->fir, &imp, 1, &sym);
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
            float _Complex imp = (state->sym_pos == 0)
                                    ? (state->cur_re + state->cur_im * I)
                                    : (0.0f + 0.0f * I);
            dp_fir_execute(state->fir, &imp, 1, &sym);
        } else {
            sym = state->cur_re + state->cur_im * I; /* rect sample-and-hold */
        }
        if (++state->sym_pos >= state->nsps)
            state->sym_pos = 0;
    } else {
        sym = state->cur_re + state->cur_im * I;
    }
    float _Complex carrier = 1.0f + 0.0f * I;
    if (state->lo) {
        dp_lo_steps(state->lo, 1, &carrier, 1);
    } else if (state->wtype == WFM_SYNTH_CHIRP) {
        /* Sweeping carrier: f(n) = f0 + k*n (normalised cycles/sample), held at
         * f_end once the span is reached. Phase accumulates in cycles, wrapped to
         * [0,1) each step so the double keeps precision over a long sweep. The
         * fused sym*carrier + noise below is the *same* expression the tone path
         * (and dp_wfm_synth_steps) uses, so step()/steps() stay byte-identical. */
        double nf = (state->chirp_span && state->chirp_n >= state->chirp_span)
                        ? (double)state->chirp_span
                        : (double)state->chirp_n;
        double w   = state->chirp_f0 + state->chirp_k * nf;
        carrier    = cexpf((float)(6.283185307179586 * state->chirp_ph) * I);
        state->chirp_ph += w;
        state->chirp_ph -= floor(state->chirp_ph);
        state->chirp_n++;
    }
    float _Complex noise = 0.0f + 0.0f * I;
    if (state->awgn)
        dp_awgn_generate(state->awgn, 1, &noise, 1);
    return sym * carrier + noise;
}

/**
 * @brief Generate a block of output samples.
 * Calls dp_wfm_synth_step() in a tight loop, writing each cf32 sample into
 * ``output``.  The Python binding returns a freshly allocated NumPy
 * complex64 array; ownership is transferred to the caller.
 *
 * @param state   Initialised Synth state returned by ``dp_wfm_synth_create``.
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
void dp_wfm_synth_steps(
    dp_wfm_synth_state_t *state,
    float _Complex          *output,
    size_t               n);

/**
 * @brief Return the active waveform type discriminant.
 * Maps to the WFM_SYNTH_* enum: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk.
 * Use this to inspect which synthesis path is active at runtime.
 *
 * @param state  Must be non-NULL.
 * @return Integer waveform type index (WFM_SYNTH_TONE .. WFM_SYNTH_QPSK).
 */
int dp_wfm_synth_get_wtype(const dp_wfm_synth_state_t *state);

/**
 * @brief Override the waveform type discriminant in-place.
 * Changing wtype does not reinitialise sub-objects; use with care.
 *
 * @param state  Must be non-NULL.
 * @param val    New wtype value (WFM_SYNTH_TONE .. WFM_SYNTH_QPSK).
 */
void dp_wfm_synth_set_wtype(dp_wfm_synth_state_t *state, int val);

/**
 * @brief Return the samples-per-symbol count.
 * For modulated types (BPSK, QPSK, PN) each symbol is held for nsps
 * consecutive output samples.  For tone/noise this field is present but
 * unused by the synthesis path.
 *
 * @param state  Must be non-NULL.
 * @return Samples per symbol (nsps >= 1).
 */
int dp_wfm_synth_get_nsps(const dp_wfm_synth_state_t *state);

/**
 * @brief Override the samples-per-symbol count in-place.
 * Does not flush the symbol-position counter (sym_pos); set sym_pos=0
 * as well when changing sps mid-stream.
 *
 * @param state  Must be non-NULL.
 * @param val    New nsps value (>= 1).
 */
void dp_wfm_synth_set_nsps(dp_wfm_synth_state_t *state, int val);

/**
 * @brief Return the current position within the current symbol (0..nsps-1).
 * Reaches nsps and wraps to 0 each time a new symbol is consumed from the
 * PN LFSR.  Useful for frame alignment: sym_pos==0 on a step boundary
 * means the very next sample begins a fresh symbol.
 *
 * @param state  Must be non-NULL.
 * @return Symbol position counter (0 <= sym_pos < nsps).
 */
int dp_wfm_synth_get_sym_pos(const dp_wfm_synth_state_t *state);

/**
 * @brief Override the symbol-position counter in-place.
 * Injecting 0 forces the next dp_wfm_synth_step() to latch a new PN chip; any
 * other value fast-forwards into the middle of the current symbol hold.
 *
 * @param state  Must be non-NULL.
 * @param val    New sym_pos value (0 <= val < nsps).
 */
void dp_wfm_synth_set_sym_pos(dp_wfm_synth_state_t *state, int val);

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
float dp_wfm_synth_get_cur_re(const dp_wfm_synth_state_t *state);

/**
 * @brief Override the held-symbol real (I) component in-place.
 * Takes effect on the next dp_wfm_synth_step() within the current symbol hold.
 *
 * @param state  Must be non-NULL.
 * @param val    New cur_re value.
 */
void dp_wfm_synth_set_cur_re(dp_wfm_synth_state_t *state, float val);

/**
 * @brief Return the imaginary part of the current held symbol.
 * For QPSK this is the Q component (±1/√2); for BPSK/PN it is always 0;
 * for tone/noise it is 0.
 *
 * @param state  Must be non-NULL.
 * @return Current symbol imaginary (Q) component.
 */
float dp_wfm_synth_get_cur_im(const dp_wfm_synth_state_t *state);

/**
 * @brief Override the held-symbol imaginary (Q) component in-place.
 * Takes effect on the next dp_wfm_synth_step() within the current symbol hold.
 *
 * @param state  Must be non-NULL.
 * @param val    New cur_im value.
 */
void dp_wfm_synth_set_cur_im(dp_wfm_synth_state_t *state, float val);



/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition of optional fir/lo/awgn/pn children (presence-flagged) +
 * running waveform-position scalars; bits/config restored by create. */
#define WFM_SYNTH_STATE_MAGIC DP_FOURCC ('W','F','M','S')
#define WFM_SYNTH_STATE_VERSION 2u /* v2: + continuous-DSSS chip/symbol clocks */
size_t dp_wfm_synth_state_bytes (const dp_wfm_synth_state_t *state);
void dp_wfm_synth_get_state (const dp_wfm_synth_state_t *state, void *blob);
int dp_wfm_synth_set_state (dp_wfm_synth_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* WFM_SYNTH_CORE_H */
