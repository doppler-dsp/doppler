#include "pn/pn_core.h"
#include "synth/synth_core.h" /* synth_mls_poly() — the primitive-poly table */
#include <complex.h>
#include <math.h>
#include <stdio.h>

#define CHECK(cond) \
    do { if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        _fails++; \
    } } while (0)

/* Floating-point helpers — use inline functions, not macros, so arguments
 * are evaluated exactly once.  Safe to call with stateful step() results. */
static inline int _almost_eq(float a, float b, float tol)
    { return fabsf(a - b) <= tol; }
static inline int _almost_eq_c(float complex a, float complex b, float tol)
    { return _almost_eq(crealf(a), crealf(b), tol)
          && _almost_eq(cimagf(a), cimagf(b), tol); }
#define ALMOST_EQ(a, b, tol)   _almost_eq((float)(a),         (float)(b),         tol)
#define ALMOST_EQ_C(a, b, tol) _almost_eq_c((float complex)(a), (float complex)(b), tol)

/* Steps the LFSR until the register returns to its seed; returns the period,
 * or -1 if it exceeds 2^n (non-maximal / degenerate). */
static long
pn_period(uint64_t poly, uint32_t n, int lfsr)
{
    pn_state_t *p = pn_create(poly, 1, n, lfsr);
    if (!p)
        return -1;
    long per = 0;
    long cap = (n >= 31) ? (1L << 31) : (1L << n) + 2;
    do {
        pn_step(p);
        per++;
        if (per > cap) {
            per = -1;
            break;
        }
    } while (p->reg != 1u);
    pn_destroy(p);
    return per;
}

int main(void)
{
    int _fails = 0;
    pn_state_t *obj = pn_create(96, 1, 7, PN_GALOIS);
    CHECK(obj != NULL);
    if (!obj) return 1;

    /* ── 64-bit register: length up to 64, mask + no truncation ── */
    CHECK(pn_create(0, 1, 65, PN_GALOIS) == NULL);  /* length > 64 rejected */
    pn_state_t *p64 = pn_create(synth_mls_poly(64), 1, 64, PN_GALOIS);
    CHECK(p64 != NULL);
    if (p64) {
        CHECK(p64->mask == ~(uint64_t)0);       /* full 64-bit mask */
        CHECK(p64->poly > 0xFFFFFFFFu);          /* 64-bit poly survived */
        int hi = 0;
        for (long i = 0; i < 300000; i++) {
            pn_step(p64);
            if (p64->reg > 0xFFFFFFFFu) hi = 1;  /* uses the high half */
            CHECK(p64->reg != 0);                /* never collapses to 0 */
        }
        CHECK(hi);
        pn_destroy(p64);
    }

    /* ── MLS table: maximal period (Galois), incl. the n > 32 path ── */
    CHECK(pn_period(synth_mls_poly(7), 7, PN_GALOIS) == (1L << 7) - 1);
    CHECK(pn_period(synth_mls_poly(17), 17, PN_GALOIS) == (1L << 17) - 1);
    CHECK(pn_period(synth_mls_poly(20), 20, PN_GALOIS) == (1L << 20) - 1);

    /* ── Fibonacci: same primitive poly → same maximal period ── */
    CHECK(pn_period(synth_mls_poly(7), 7, PN_FIBONACCI) == (1L << 7) - 1);
    CHECK(pn_period(synth_mls_poly(17), 17, PN_FIBONACCI) == (1L << 17) - 1);
    CHECK(pn_period(synth_mls_poly(20), 20, PN_FIBONACCI) == (1L << 20) - 1);

    /* ── Galois and Fibonacci are distinct realizations (different chips) ── */
    {
        pn_state_t *g = pn_create(synth_mls_poly(9), 1, 9, PN_GALOIS);
        pn_state_t *f = pn_create(synth_mls_poly(9), 1, 9, PN_FIBONACCI);
        int diff = 0;
        for (int i = 0; i < 511; i++)
            if (pn_step(g) != pn_step(f)) diff = 1;
        CHECK(diff); /* same period, different sequence/phase */
        pn_destroy(g);
        pn_destroy(f);
    }

    /* ── table coverage: nonzero for 2..64, zero outside ── */
    CHECK(synth_mls_poly(1) == 0);
    CHECK(synth_mls_poly(65) == 0);
    for (uint32_t n = 2; n <= 64; n++)
        CHECK(synth_mls_poly(n) != 0);

    /* reset */
    pn_reset(obj);

    pn_destroy(obj);
    if (_fails) {
        fprintf(stderr, "test_pn_core FAILED (%d)\n", _fails);
        return 1;
    }
    printf("test_pn_core PASSED\n");
    return 0;
}
