/**
 * @file cic_core.c
 * @brief CIC decimation filter — uint64_t modular-arithmetic pipelines.
 *
 * Two independent pipelines (real, imaginary) share identical logic.
 * All accumulation is uint64_t so C-guaranteed unsigned wrap-around
 * handles intermediate overflow without any saturation or range checks.
 *
 * The modular-overflow property of CIC filters guarantees correctness:
 * every overflow in the integrators is exactly cancelled by the comb
 * subtractions, provided the true final result fits in 63 bits.
 */

#include "cic/cic_core.h"

#include <math.h>

/* ── helpers ──────────────────────────────────────────────────────────── */

/*
 * Maximum input_scale (a power of 2) such that scale × (R×M)^N < 2^63.
 *
 * We use power-of-2 scales because they are always exactly representable
 * as doubles.  Non-power-of-2 scales (e.g., INT64_MAX / gain when gain
 * is a power of 2) often require more than 53 significant bits and round
 * up, pushing scale × gain to exactly 2^63 which overflows int64_t.
 *
 * k = floor(63 - log2(gain) - ε)   where ε = 1e-9 handles the case
 * where 63 - log2(gain) is exactly integer (power-of-2 gains), giving
 * strict inequality scale × gain < 2^63.
 */
static double
max_scale(uint32_t R, uint32_t N, uint32_t M)
{
    double log2_gain = (double)N * log2((double)R * (double)M);
    int k = (int)(63.0 - log2_gain - 1e-9);
    if (k <= 0)
        return 1.0;
    return ldexp(1.0, k);
}

static void
alloc_comb(uint32_t N, uint32_t M,
           uint64_t **re_out, uint64_t **im_out)
{
    *re_out = (uint64_t *)calloc((size_t)N * M, sizeof(uint64_t));
    *im_out = (uint64_t *)calloc((size_t)N * M, sizeof(uint64_t));
}

/* ── lifecycle ────────────────────────────────────────────────────────── */

cic_state_t *
cic_create(uint32_t R, uint32_t N, uint32_t M)
{
    if (R == 0 || N == 0 || N > 6 || M == 0 || M > 2)
        return NULL;

    cic_state_t *s = (cic_state_t *)calloc(1, sizeof(*s));
    if (!s)
        return NULL;

    alloc_comb(N, M, &s->comb_re, &s->comb_im);
    if (!s->comb_re || !s->comb_im)
    {
        free(s->comb_re);
        free(s->comb_im);
        free(s);
        return NULL;
    }

    s->R = R;
    s->N = N;
    s->M = M;
    s->input_scale  = max_scale(R, N, M);
    s->output_scale = 1.0 / (s->input_scale * pow((double)R * M, (double)N));
    return s;
}

void
cic_destroy(cic_state_t *state)
{
    if (!state)
        return;
    free(state->comb_re);
    free(state->comb_im);
    free(state);
}

void
cic_reset(cic_state_t *state)
{
    memset(state->integ_re, 0, sizeof(state->integ_re));
    memset(state->integ_im, 0, sizeof(state->integ_im));
    memset(state->comb_re,  0, state->N * state->M * sizeof(uint64_t));
    memset(state->comb_im,  0, state->N * state->M * sizeof(uint64_t));
    memset(state->comb_head, 0, sizeof(state->comb_head));
    state->phase = 0;
}

/* ── decimate ─────────────────────────────────────────────────────────── */

size_t
cic_decimate_max_out(cic_state_t *state)
{
    /* Return 0: the ext layer allocates n_in on first call (lazy path).
     * n_in is always ≥ n_out = n_in/R, so the buffer is always large
     * enough for consistent block sizes.  See cic_core.h for details. */
    (void)state;
    return 0;
}

JM_HOT size_t
cic_decimate(cic_state_t *state, const float complex *in,
             size_t n_in, float complex *out)
{
    const uint32_t R      = state->R;
    const uint32_t N      = state->N;
    const uint32_t M      = state->M;
    const double   iscale = state->input_scale;
    const double   oscale = state->output_scale;

    size_t n_out = 0;

    for (size_t i = 0; i < n_in; i++)
    {
        /* CF32 → uint64_t: scale then reinterpret as two's complement. */
        uint64_t re = (uint64_t)(int64_t)(crealf(in[i]) * iscale);
        uint64_t im = (uint64_t)(int64_t)(cimagf(in[i]) * iscale);

        /* N integrators in series.  Each stage feeds its output to the
         * next.  uint64_t wrap-around is defined and intentional. */
        for (uint32_t k = 0; k < N; k++)
        {
            re = state->integ_re[k] += re;
            im = state->integ_im[k] += im;
        }

        /* Decimate: advance phase; emit only every R-th sample. */
        if (++state->phase < R)
            continue;
        state->phase = 0;

        /* N comb stages: y[n] = x[n] - x[n-M].
         * comb_re/im[k*M .. k*M+M-1] is a circular buffer of M entries.
         * comb_head[k] always points to the oldest entry (M steps back). */
        for (uint32_t k = 0; k < N; k++)
        {
            uint32_t head = state->comb_head[k];
            size_t   idx  = (size_t)k * M + head;

            uint64_t old_re = state->comb_re[idx];
            uint64_t old_im = state->comb_im[idx];
            state->comb_re[idx] = re;
            state->comb_im[idx] = im;
            state->comb_head[k] = (head + 1 < M) ? head + 1 : 0;

            re -= old_re;  /* unsigned subtraction wraps correctly */
            im -= old_im;
        }

        /* uint64_t → CF32: reinterpret as int64_t (signed), then scale. */
        out[n_out++] = CMPLXF(
            (float)((double)(int64_t)re * oscale),
            (float)((double)(int64_t)im * oscale));
    }

    return n_out;
}

/* ── reconfigure ──────────────────────────────────────────────────────── */

void
cic_reconfigure(cic_state_t *state, uint32_t R, uint32_t N, uint32_t M)
{
    if (R == 0 || N == 0 || N > 6 || M == 0 || M > 2)
        return;

    /* Reallocate comb delay lines only if N×M actually changes. */
    if (N != state->N || M != state->M)
    {
        uint64_t *new_re, *new_im;
        alloc_comb(N, M, &new_re, &new_im);
        if (!new_re || !new_im)
        {
            free(new_re);
            free(new_im);
            return; /* keep existing config on OOM */
        }
        free(state->comb_re);
        free(state->comb_im);
        state->comb_re = new_re;
        state->comb_im = new_im;
    }

    state->R = R;
    state->N = N;
    state->M = M;
    state->input_scale  = max_scale(R, N, M);
    state->output_scale = 1.0 / (state->input_scale
                                 * pow((double)R * M, (double)N));

    /* Reset all filter state for the new configuration. */
    memset(state->integ_re,  0, sizeof(state->integ_re));
    memset(state->integ_im,  0, sizeof(state->integ_im));
    memset(state->comb_re,   0, (size_t)N * M * sizeof(uint64_t));
    memset(state->comb_im,   0, (size_t)N * M * sizeof(uint64_t));
    memset(state->comb_head, 0, sizeof(state->comb_head));
    state->phase = 0;
}
