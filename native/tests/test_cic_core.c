/* test_cic_core.c — CIC decimation filter unit tests.
 *
 * Covers:
 *   - Invalid constructor arguments → NULL
 *   - Output sample count: n_in/R for any block size multiple of R
 *   - DC response: settled output = 1.0 for ±1.0 DC input, real and complex
 *   - Zero input: output is exactly 0+0j throughout
 *   - Reset: second run with same input produces byte-identical output
 *   - Reconfigure: output count and DC response correct after R/N/M change
 *   - M=2 differential delay variant
 *   - cic_destroy(NULL): no crash
 */
#include "cic/cic_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(cond) \
    do { if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        _fails++; \
    } } while (0)

static inline int _feq(float a, float b, float tol)
    { return fabsf(a - b) <= tol; }
static inline int _ceq(float complex a, float complex b, float tol)
    { return _feq(crealf(a), crealf(b), tol)
          && _feq(cimagf(a), cimagf(b), tol); }
#define FEQC(a, b, tol) _ceq((float complex)(a), (float complex)(b), (tol))

/*
 * Feed n_in copies of `sample` through obj and return the last output.
 * `out` must hold at least ceil(n_in / R) elements.
 * Caller guarantees n_in is large enough that the filter has settled.
 */
static float complex
dc_last(cic_state_t *obj, float complex sample,
        float complex *out, size_t n_in)
{
    float complex *in = (float complex *)malloc(n_in * sizeof(float complex));
    for (size_t i = 0; i < n_in; i++)
        in[i] = sample;
    size_t n = cic_decimate(obj, in, n_in, out);
    free(in);
    return out[n - 1];
}

int main(void)
{
    int _fails = 0;

    /* ── Invalid constructor args → NULL ─────────────────────────────────── */
    CHECK(cic_create(0, 4, 1) == NULL);  /* R = 0 */
    CHECK(cic_create(4, 0, 1) == NULL);  /* N = 0 */
    CHECK(cic_create(4, 7, 1) == NULL);  /* N > 6 */
    CHECK(cic_create(4, 4, 0) == NULL);  /* M = 0 */
    CHECK(cic_create(4, 4, 3) == NULL);  /* M > 2 */

    /* NULL destroy is a documented no-op */
    cic_destroy(NULL);

    /* ── Output sample count ─────────────────────────────────────────────── */
    /* For a fresh filter, n_in = k*R must produce exactly k outputs. */
    {
        uint32_t R = 8;
        cic_state_t *obj = cic_create(R, 3, 1);
        CHECK(obj != NULL);
        float complex in[256] = {0}, out[256];
        for (int k = 1; k <= 4; k++) {
            cic_reset(obj);
            size_t n = cic_decimate(obj, in, (size_t)k * R, out);
            CHECK(n == (size_t)k);
        }
        /* Partial block: R-1 inputs → 0 outputs */
        cic_reset(obj);
        CHECK(cic_decimate(obj, in, R - 1, out) == 0);
        /* Then 1 more input completes the first decimation cycle */
        CHECK(cic_decimate(obj, in, 1, out) == 1);
        cic_destroy(obj);
    }

    /* ── Zero input → exactly zero output ───────────────────────────────── */
    {
        cic_state_t *obj = cic_create(4, 2, 1);
        CHECK(obj != NULL);
        float complex in[64] = {0}, out[64];
        size_t n = cic_decimate(obj, in, 64, out);
        CHECK(n == 16);
        for (size_t i = 0; i < n; i++)
            CHECK(FEQC(out[i], 0.0f, 0.0f));
        cic_destroy(obj);
    }

    /* ── DC response: +1.0 real ──────────────────────────────────────────── */
    /* Transient ≈ N*(R-1) input samples; use 8*R*N to ensure full settling. */
    {
        cic_state_t *obj = cic_create(4, 2, 1);
        CHECK(obj != NULL);
        float complex out[256];
        float complex last = dc_last(obj, 1.0f + 0.0f * I, out, 8 * 4 * 2);
        CHECK(FEQC(last, 1.0f + 0.0f * I, 1e-5f));
        cic_destroy(obj);
    }

    /* ── DC response: −1.0 real (tests signed two's-complement path) ─────── */
    {
        cic_state_t *obj = cic_create(4, 2, 1);
        CHECK(obj != NULL);
        float complex out[256];
        float complex last = dc_last(obj, -1.0f + 0.0f * I, out, 8 * 4 * 2);
        CHECK(FEQC(last, -1.0f + 0.0f * I, 1e-5f));
        cic_destroy(obj);
    }

    /* ── DC response: +j (imaginary path independent of real) ────────────── */
    {
        cic_state_t *obj = cic_create(4, 2, 1);
        CHECK(obj != NULL);
        float complex out[256];
        float complex last = dc_last(obj, 0.0f + 1.0f * I, out, 8 * 4 * 2);
        CHECK(FEQC(last, 0.0f + 1.0f * I, 1e-5f));
        cic_destroy(obj);
    }

    /* ── DC response: (0.5 + 0.5j) — typical SDR config R=32 N=4 ─────────── */
    /* Transient ≈ 4*(32-1) = 124 inputs ≈ 4 outputs; use 12*R outputs */
    {
        cic_state_t *obj = cic_create(32, 4, 1);
        CHECK(obj != NULL);
        float complex out[512];
        size_t n_in = 12 * 32;
        float complex last = dc_last(obj, 0.5f + 0.5f * I, out, n_in);
        CHECK(FEQC(last, 0.5f + 0.5f * I, 1e-5f));
        cic_destroy(obj);
    }

    /* ── M=2 differential delay ──────────────────────────────────────────── */
    {
        cic_state_t *obj = cic_create(8, 3, 2);
        CHECK(obj != NULL);
        float complex out[512];
        /* Transient ≈ N*(R-1)*M = 3*7*2 = 42 inputs; use 8*R outputs */
        float complex last = dc_last(obj, 1.0f + 0.0f * I, out, 8 * 8);
        CHECK(FEQC(last, 1.0f + 0.0f * I, 1e-5f));
        cic_destroy(obj);
    }

    /* ── Reset: second run produces byte-identical output ────────────────── */
    {
        cic_state_t *obj = cic_create(4, 2, 1);
        CHECK(obj != NULL);
        size_t n_in = 64;
        float complex *in  = malloc(n_in * sizeof(float complex));
        float complex *out1 = malloc(n_in * sizeof(float complex));
        float complex *out2 = malloc(n_in * sizeof(float complex));
        /* non-trivial input: ramp on real, constant on imag */
        for (size_t i = 0; i < n_in; i++)
            in[i] = (float)i * 0.01f + 0.5f * I;

        size_t n1 = cic_decimate(obj, in, n_in, out1);
        cic_reset(obj);
        size_t n2 = cic_decimate(obj, in, n_in, out2);
        CHECK(n1 == n2);
        CHECK(memcmp(out1, out2, n1 * sizeof(float complex)) == 0);

        free(in); free(out1); free(out2);
        cic_destroy(obj);
    }

    /* ── Reconfigure: output count and DC response update correctly ──────── */
    {
        cic_state_t *obj = cic_create(4, 2, 1);
        CHECK(obj != NULL);
        float complex in[256], out[256];
        for (int i = 0; i < 256; i++) in[i] = 1.0f;

        /* warm up with R=4 */
        cic_decimate(obj, in, 32, out);

        /* reconfigure to R=8, N=3 */
        cic_reconfigure(obj, 8, 3, 1);

        /* output count must reflect new R */
        size_t n = cic_decimate(obj, in, 8 * 8 * 3, out);
        CHECK(n == (size_t)(8 * 3));

        /* settled output must be 1.0 */
        CHECK(FEQC(out[n - 1], 1.0f + 0.0f * I, 1e-5f));
        cic_destroy(obj);
    }

    /* ── Reconfigure: invalid args are silently ignored ─────────────────── */
    {
        cic_state_t *obj = cic_create(4, 2, 1);
        CHECK(obj != NULL);
        cic_reconfigure(obj, 0, 2, 1);   /* R=0: invalid, ignored */
        CHECK(obj->R == 4);              /* unchanged */
        cic_reconfigure(obj, 4, 7, 1);   /* N=7 > 6: invalid, ignored */
        CHECK(obj->N == 2);              /* unchanged */
        cic_destroy(obj);
    }

    /* ── Streaming: split block across two calls ─────────────────────────── */
    /* Two calls of R samples must give same result as one call of 2R samples */
    {
        uint32_t R = 16;
        float complex in[64], out_split[4], out_whole[4];
        for (int i = 0; i < 64; i++) in[i] = 0.7f - 0.3f * I;

        cic_state_t *a = cic_create(R, 3, 1);
        cic_state_t *b = cic_create(R, 3, 1);
        CHECK(a && b);

        /* whole: 2R in one call */
        cic_decimate(b, in, 2 * R, out_whole);

        /* split: R then R */
        cic_decimate(a, in,       R, out_split);
        cic_decimate(a, in + R,   R, out_split + 1);

        CHECK(FEQC(out_split[0], out_whole[0], 0.0f));
        CHECK(FEQC(out_split[1], out_whole[1], 0.0f));

        cic_destroy(a);
        cic_destroy(b);
    }

    if (_fails) {
        fprintf(stderr, "test_cic_core FAILED (%d)\n", _fails);
        return 1;
    }
    printf("test_cic_core PASSED\n");
    return 0;
}
