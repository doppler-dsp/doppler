#include "synth/synth_core.h"

synth_state_t *
synth_create(int type, double fs, double freq, double snr, int snr_mode, uint32_t seed, int sps, int pn_length, uint64_t pn_poly, int lfsr)
{
    synth_state_t *obj = calloc(1, sizeof(*obj));
    if (!obj)
        return NULL;
    obj->wtype = type;
    obj->nsps = (sps < 1) ? 1 : sps;
    obj->sym_pos = 0;
    obj->cur_re = (type == SYNTH_TONE) ? 1.0f : 0.0f;
    obj->cur_im = 0.0f;

    /* LO carrier for everything but pure noise (freq is Hz → cycles/sample) */
    if (type != SYNTH_NOISE) {
        obj->lo = lo_create(fs != 0.0 ? freq / fs : 0.0);
        if (!obj->lo) {
            free(obj);
            return NULL;
        }
    }

    /* PN chip/data source for pn/bpsk/qpsk; poly 0 → MLS poly for the length */
    if (type >= SYNTH_PN) {
        uint64_t poly = pn_poly ? pn_poly : synth_mls_poly((uint32_t)pn_length);
        if (poly == 0) { /* no MLS table entry for this length */
            if (obj->lo)
                lo_destroy(obj->lo);
            free(obj);
            return NULL;
        }
        obj->pn = pn_create(poly, seed ? seed : 1u, (uint32_t)pn_length, lfsr);
        if (!obj->pn) {
            if (obj->lo)
                lo_destroy(obj->lo);
            free(obj);
            return NULL;
        }
    }

    /* AWGN at the resolved SNR. snr_mode: 0 auto, 1 fs, 2 ebno, 3 esno. */
    if (type == SYNTH_NOISE) {
        obj->awgn = awgn_create((uint64_t)seed, (float)(1.0 / 1.4142135623730951));
    } else {
        int mode = snr_mode;
        if (mode == 0)
            mode = (type >= SYNTH_BPSK) ? 3 : 1; /* *psk → esno, tone/pn → fs */
        int bps = (type == SYNTH_QPSK) ? 2 : 1;
        double snr_fs;
        if (mode == 2) /* Eb/No → SNR over fs */
            snr_fs = snr + 10.0 * log10((double)bps)
                     - 10.0 * log10((double)obj->nsps);
        else if (mode == 3) /* Es/No → SNR over fs (Es spans nsps samples) */
            snr_fs = snr - 10.0 * log10((double)obj->nsps);
        else /* over fs */
            snr_fs = snr;
        float amp = sqrtf(1.0f / (2.0f * powf(10.0f, (float)snr_fs / 10.0f)));
        obj->awgn = awgn_create((uint64_t)seed, amp);
    }
    if (!obj->awgn) {
        if (obj->pn)
            pn_destroy(obj->pn);
        if (obj->lo)
            lo_destroy(obj->lo);
        free(obj);
        return NULL;
    }
    return obj;
}

void
synth_destroy(synth_state_t *state)
{
    if (state->lo)
        lo_destroy(state->lo);
    if (state->awgn)
        awgn_destroy(state->awgn);
    if (state->pn)
        pn_destroy(state->pn);
    free(state);
}

void
synth_reset(synth_state_t *state)
{
    state->sym_pos = 0;
    state->cur_re = (state->wtype == SYNTH_TONE) ? 1.0f : 0.0f;
    state->cur_im = 0.0f;
    if (state->lo)
        lo_reset(state->lo);
    if (state->awgn)
        awgn_reset(state->awgn);
    if (state->pn)
        pn_reset(state->pn);
}

void synth_steps(
    synth_state_t *state,
    float complex          *output,
    size_t               n)
{
    /* Batched generation: the LO carrier (NCO) and AWGN are produced a block at
     * a time so their vectorized (libmvec / AVX-512) paths run, instead of one
     * sample per call. Only the symbol timing (PN/PSK) stays sequential — it is
     * cheap integer work — and the final multiply/add is data-parallel. Output
     * is identical to a per-sample synth_step() loop. */
    enum { CH = 2048 }; /* <= LO_MAX_OUT; stack carrier+noise = 32 KiB */
    float complex carrier[CH];
    float complex noise[CH];
    const int has_lo = state->lo != NULL;
    const int has_awgn = state->awgn != NULL;
    const int modulated = state->wtype >= SYNTH_PN;
    const int qpsk = state->wtype == SYNTH_QPSK;
    const int nsps = state->nsps;
    int sym_pos = state->sym_pos;
    float cre = state->cur_re, cim = state->cur_im;

    for (size_t done = 0; done < n;) {
        size_t m = (n - done < (size_t)CH) ? (n - done) : (size_t)CH;
        if (has_lo)
            lo_steps(state->lo, m, carrier);
        if (has_awgn)
            awgn_generate(state->awgn, m, noise);
        float complex *out = output + done;
        for (size_t i = 0; i < m; i++) {
            if (modulated && sym_pos == 0) {
                if (qpsk) {
                    uint8_t b0 = pn_step(state->pn);
                    uint8_t b1 = pn_step(state->pn);
                    const float s = 0.70710678118654752f;
                    cre = b0 ? -s : s;
                    cim = b1 ? -s : s;
                } else {
                    uint8_t b = pn_step(state->pn);
                    cre = b ? -1.0f : 1.0f;
                    cim = 0.0f;
                }
            }
            if (modulated && ++sym_pos >= nsps)
                sym_pos = 0;
            float complex sym = cre + cim * I;
            float complex v = has_lo ? sym * carrier[i] : sym;
            out[i] = has_awgn ? v + noise[i] : v;
        }
        done += m;
    }
    state->sym_pos = sym_pos;
    state->cur_re = cre;
    state->cur_im = cim;
}

int
synth_get_wtype(const synth_state_t *state)
{
    return state->wtype;
}

void
synth_set_wtype(synth_state_t *state, int val)
{
    state->wtype = val;
}

int
synth_get_nsps(const synth_state_t *state)
{
    return state->nsps;
}

void
synth_set_nsps(synth_state_t *state, int val)
{
    state->nsps = val;
}

int
synth_get_sym_pos(const synth_state_t *state)
{
    return state->sym_pos;
}

void
synth_set_sym_pos(synth_state_t *state, int val)
{
    state->sym_pos = val;
}

float
synth_get_cur_re(const synth_state_t *state)
{
    return state->cur_re;
}

void
synth_set_cur_re(synth_state_t *state, float val)
{
    state->cur_re = val;
}

float
synth_get_cur_im(const synth_state_t *state)
{
    return state->cur_im;
}

void
synth_set_cur_im(synth_state_t *state, float val)
{
    state->cur_im = val;
}
