/**
 * @file test_hbdecim_q15_core.c
 * @brief Unit tests for the Q15 halfband decimator (structural / C-level).
 *
 * Frequency-domain and SNR tests live in the Python suite where scipy
 * can generate valid halfband prototypes.  These tests cover lifecycle,
 * zero-input passthrough, decimation ratio, odd-block buffering, and
 * reset behaviour — all verifiable without a filter-design library.
 */
#include "hbdecim_q15/hbdecim_q15_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CHECK(cond) \
    do { if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        _fails++; \
    } } while (0)

/* Flat impulse response (all ones, num_taps=1): FIR branch is empty
 * (K=0), only the delay center-tap path executes.  Useful for testing
 * the pure-delay branch and the 2:1 output ratio in isolation.        */
static const float H1[1] = { 1.0f };

/* Larger coefficient array for structural tests (25 taps).
 * Values don't form an optimal halfband — only zero-input is tested.  */
static const float H25_stub[25] = {
    0.01f, 0.0f, -0.02f, 0.0f, 0.03f, 0.0f, -0.04f, 0.0f,
    0.08f, 0.0f, -0.20f, 0.0f,  0.50f, 0.0f, -0.20f, 0.0f,
    0.08f, 0.0f, -0.04f, 0.0f,  0.03f, 0.0f, -0.02f, 0.0f, 0.01f
};

int main(void)
{
    int _fails = 0;

    /* ── NULL / bad-arg guards ───────────────────────────────────── */
    CHECK(hbdecim_q15_create(0,   H1  ) == NULL);
    CHECK(hbdecim_q15_create(1,   NULL) == NULL);

    /* ── Lifecycle (num_taps=1) ──────────────────────────────────── */
    hbdecim_q15_state_t *r = hbdecim_q15_create(1, H1);
    CHECK(r != NULL);
    if (!r) return 1;
    CHECK(hbdecim_q15_get_num_taps(r) == 1);
    CHECK(fabs(hbdecim_q15_get_rate(r) - 0.5) < 1e-9);
    hbdecim_q15_reset(r);
    hbdecim_q15_destroy(r);
    r = NULL;

    /* ── Zero input → zero output ────────────────────────────────── */
    r = hbdecim_q15_create(25, H25_stub);
    CHECK(r != NULL);
    if (!r) return 1;

    static const int16_t zeros[512] = {0};
    int16_t out[256];
    size_t n;

    n = hbdecim_q15_execute(r, zeros, 256, out, 128);
    CHECK(n == 128);
    for (int i = 0; i < 256; i++)
        CHECK(out[i] == 0);

    /* ── 2:1 decimation ratio ────────────────────────────────────── */
    int16_t ramp[2048];
    int16_t ramp_out[1024];
    for (int i = 0; i < 2048; i++) ramp[i] = (int16_t)(i & 0x7fff);
    n = hbdecim_q15_execute(r, ramp, 1024, ramp_out, 512);
    CHECK(n == 512);

    /* ── Odd block: trailing even pair buffered, consumed next call ─ */
    hbdecim_q15_reset(r);
    n = hbdecim_q15_execute(r, zeros, 3, out, 64);
    CHECK(n == 1);   /* floor(3/2) = 1 complete pair processed */
    n = hbdecim_q15_execute(r, zeros, 1, out, 64);
    CHECK(n == 1);   /* buffered pair + 1 new odd = 1 more output   */

    /* ── Execute with empty input ────────────────────────────────── */
    n = hbdecim_q15_execute(r, zeros, 0, out, 64);
    CHECK(n == 0);

    /* ── max_out=0 produces no output ───────────────────────────── */
    n = hbdecim_q15_execute(r, zeros, 128, out, 0);
    CHECK(n == 0);

    /* ── reset clears delay lines (zero after reset + zero input) ── */
    hbdecim_q15_reset(r);
    n = hbdecim_q15_execute(r, zeros, 256, out, 128);
    CHECK(n == 128);
    for (int i = 0; i < 256; i++)
        CHECK(out[i] == 0);

    hbdecim_q15_destroy(r);

    /* ── execute_max_out always returns 0 (lazy-alloc signal) ────── */
    r = hbdecim_q15_create(1, H1);
    CHECK(r != NULL);
    if (!r) return 1;
    CHECK(hbdecim_q15_execute_max_out(r) == 0);
    hbdecim_q15_destroy(r);

    if (_fails) {
        fprintf(stderr, "test_hbdecim_q15_core FAILED (%d)\n", _fails);
        return 1;
    }
    printf("test_hbdecim_q15_core PASSED\n");
    return 0;
}
