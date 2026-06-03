/**
 * @file hbdecim_q15_core.c
 * @brief Fixed-point halfband 2:1 decimator — Q15 IQ implementation.
 *
 * See hbdecim_q15_core.h for the full algorithm description.
 */
#include "hbdecim_q15/hbdecim_q15_core.h"
#include "q15_mac.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================== */
/* Delay-line helpers (dual-write circular buffers)                   */
/* ================================================================== */

static inline void
push_even(hbdecim_q15_state_t *r, int16_t i, int16_t q)
{
    r->even_head = (r->even_head - 1) & r->mask;
    r->even_I[r->even_head]           = i;
    r->even_I[r->even_head + r->cap]  = i;
    r->even_Q[r->even_head]           = q;
    r->even_Q[r->even_head + r->cap]  = q;
}

static inline void
push_odd(hbdecim_q15_state_t *r, int16_t i, int16_t q)
{
    r->odd_head = (r->odd_head - 1) & r->mask;
    r->odd_I[r->odd_head]          = i;
    r->odd_I[r->odd_head + r->cap] = i;
    r->odd_Q[r->odd_head]          = q;
    r->odd_Q[r->odd_head + r->cap] = q;
}

/* ================================================================== */
/* SIMD helpers (AVX2)                                                */
/* ================================================================== */

#if defined(__AVX2__)

/* Reverse 16 int16_t within a 256-bit register:
 *   in:  a0  a1  a2  a3  a4  a5  a6  a7  a8  a9  a10 a11 a12 a13 a14 a15
 *   out: a15 a14 a13 a12 a11 a10 a9  a8  a7  a6  a5  a4  a3  a2  a1  a0
 * Strategy: reverse bytes within each 128-bit lane, then swap lanes.   */
static inline __m256i
rev16_256(__m256i v)
{
    /* Byte-reverse mask for one 128-bit lane of 8 int16_t elements.
     * Each int16 occupies 2 bytes; reversing the element order also
     * reverses the byte order of each element — correct the byte swap
     * within each int16 by interleaving pairs: swap bytes at 0,1 → 1,0. */
    static const int8_t _bm[16] = {
        14,15, 12,13, 10,11, 8,9, 6,7, 4,5, 2,3, 0,1
    };
    const __m128i bm = _mm_loadu_si128((const __m128i *)_bm);
    __m128i lo = _mm256_extracti128_si256(v, 0);
    __m128i hi = _mm256_extracti128_si256(v, 1);
    lo = _mm_shuffle_epi8(lo, bm);
    hi = _mm_shuffle_epi8(hi, bm);
    /* Swap lanes: old-hi → lane 0, old-lo → lane 1. */
    return _mm256_inserti128_si256(_mm256_castsi128_si256(hi), lo, 1);
}

/*
 * fir_q15_avx2 — inner dot product for one channel (I or Q).
 *
 * a[0..N-1]:        contiguous slice of the dual-write delay ring.
 * N:                fold span (num_taps for N-even, num_taps-1 for N-odd).
 * coeffs[0..Kp-1]:  K_pad Q15 coefficients, zero-padded, 32-byte aligned.
 * Kp:               K_pad (multiple of 16).
 *
 * Returns the int64_t dot product (before center-tap addition and rounding).
 *
 * Algorithm — two-pass (no explicit fold):
 *   pass 1: madd16(left,  cv)  where left[k]  = a[k]
 *   pass 2: madd16(rrev,  cv)  where rrev[k]  = a[N-1-k]  (right, reversed)
 *   sum = pass1 + pass2 = Σ cv[k] × (a[k] + a[N-1-k])
 *
 * This avoids computing the explicit fold (a[k]+a[N-1-k]) as an int16_t,
 * which overflows at inputs above -6 dBFS (sum of two in-phase samples
 * exceeds 32767).  Each madd operand stays within int16_t range, and the
 * two 8-wide int32 partial sums stay within int32_t for all practical
 * halfband coefficient magnitudes (max |h| < 0.65 → max |coeff_q15| < 21299
 * after ×0.5 scaling, giving max per-lane = 2×21299×32767 < 2^31).
 */
static int64_t
fir_q15_avx2(const int16_t *a, size_t N, const int16_t *coeffs, size_t Kp)
{
    __m256i acc = _mm256_setzero_si256();

    for (size_t k = 0; k < Kp; k += 16) {
        /* Left side: a[k .. k+15] in forward order. */
        __m256i left = _mm256_loadu_si256((const __m256i *)(a + k));
        /* Right side: a[N-16-k .. N-1-k] loaded then reversed
         * so rrev[j] = a[N-1-k-j] — the mirror of left[j]. */
        __m256i rraw = _mm256_loadu_si256((const __m256i *)(a + N - 16 - k));
        __m256i rrev = rev16_256(rraw);
        __m256i cv   = _mm256_load_si256((const __m256i *)(coeffs + k));
        /* Two madd passes: left×cv and right×cv, both into int32 lanes.
         * Equivalent to (left+right)×cv but without the int16 fold overflow. */
        acc = _mm256_add_epi32(acc, _mm256_madd_epi16(left, cv));
        acc = _mm256_add_epi32(acc, _mm256_madd_epi16(rrev, cv));
    }
    return hsum_epi32_i64(acc);
}

#endif /* __AVX2__ */

/* ================================================================== */
/* Scalar fallback                                                     */
/* ================================================================== */

static inline int64_t
fir_q15_scalar(const int16_t *a, size_t N, const int16_t *coeffs, size_t K)
{
    int64_t acc = 0;
    for (size_t k = 0; k < K; k++) {
        int32_t fold = (int32_t)a[k] + (int32_t)a[N - 1 - k];
        acc += (int64_t)coeffs[k] * fold;
    }
    return acc;
}

/* ================================================================== */
/* Per output sample: symmetric FIR + pure-delay center tap           */
/* ================================================================== */

static inline void
compute_output(const hbdecim_q15_state_t *r, int16_t *out_I, int16_t *out_Q)
{
    const int16_t *fi, *fq; /* FIR-branch delay line (I and Q)   */
    const int16_t *di, *dq; /* Delay-branch delay line (I and Q) */
    size_t fh, dh;          /* heads for the two rings            */

    if (r->fir_on_even) {
        fi = r->even_I;  fq = r->even_Q;  fh = r->even_head;
        di = r->odd_I;   dq = r->odd_Q;   dh = r->odd_head;
    } else {
        fi = r->odd_I;   fq = r->odd_Q;   fh = r->odd_head;
        di = r->even_I;  dq = r->even_Q;  dh = r->even_head;
    }

    /* For the N-odd case the FIR branch starts at delay-line offset +1
     * (polyphase identity: x[2m-2k-1] = odd_dl[head+k+1] after push).
     * Mirrors the o[k+1] indexing in hbdecim_core.c for N odd.          */
    size_t fir_off = r->fir_on_even ? 0 : 1;
    const int16_t *a_I = fi + fh + fir_off;
    const int16_t *a_Q = fq + fh + fir_off;

    /* For the N-odd FIR path (fir_off=1), the fold spans positions
     * a[0]..a[N-2] (length N-1), pairing a[k] with a[N-2-k].
     * Equivalently: pass N_fold = N-1 so a[N_fold-1-k] = o[N-1-k]. */
    size_t N_fold = r->fir_on_even ? r->num_taps : r->num_taps - 1;

    int64_t acc_I, acc_Q;

#if defined(__AVX2__)
    acc_I = fir_q15_avx2(a_I, N_fold, r->coeffs, r->K_pad);
    acc_Q = fir_q15_avx2(a_Q, N_fold, r->coeffs, r->K_pad);
#else
    acc_I = fir_q15_scalar(a_I, N_fold, r->coeffs, r->K);
    acc_Q = fir_q15_scalar(a_Q, N_fold, r->coeffs, r->K);
#endif

    /* Center tap: delay-branch sample at 'centre' position, x0.5 (>>1).
     * The polyphase rate identity already applied x0.5 to the FIR coeffs;
     * the delay branch always carries exactly 0.5 of the center sample.  */
    acc_I += (int64_t)(di + dh)[r->centre] * (int64_t)16384;
    acc_Q += (int64_t)(dq + dh)[r->centre] * (int64_t)16384;

    /* Round and shift Q15 accumulator back to Q15 output.
     * acc is in Q(15+15) = Q30 relative to the Q15 input, with the
     * extra x0.5 from polyphase scaling making it Q31.  Shift by 15 to
     * recover Q15, rounding to nearest.                                  */
    acc_I += (1 << 14);
    acc_Q += (1 << 14);
    int32_t ri = (int32_t)(acc_I >> 15);
    int32_t rq = (int32_t)(acc_Q >> 15);
    /* Saturate to int16_t range. */
    *out_I = ri > 32767 ? 32767 : ri < -32768 ? -32768 : (int16_t)ri;
    *out_Q = rq > 32767 ? 32767 : rq < -32768 ? -32768 : (int16_t)rq;
}

/* ================================================================== */
/* Lifecycle                                                           */
/* ================================================================== */

hbdecim_q15_state_t *
hbdecim_q15_create(size_t num_taps, const float *h)
{
    if (!num_taps || !h)
        return NULL;

    hbdecim_q15_state_t *r = calloc(1, sizeof(*r));
    if (!r)
        return NULL;

    r->num_taps    = num_taps;
    r->K           = num_taps / 2;
    r->centre      = num_taps / 2;
    r->fir_on_even = !(num_taps & 1); /* even num_taps → FIR on even inputs */

    /* Pad K to multiple of 16 for AVX2 aligned coefficient loads. */
    r->K_pad = (r->K + 15) & ~(size_t)15;

    /* Allocate coefficient array aligned to 32 bytes (AVX2 requirement).
     * Zero-padding fills positions K..K_pad-1 so SIMD reads past K are
     * harmless (multiply by zero).                                        */
#ifdef _WIN32
    r->coeffs = (int16_t *)_aligned_malloc(r->K_pad * sizeof(int16_t), 32);
#else
    if (posix_memalign((void **)&r->coeffs, 32,
                       r->K_pad * sizeof(int16_t)) != 0) {
        free(r);
        return NULL;
    }
#endif
    memset(r->coeffs, 0, r->K_pad * sizeof(int16_t));

    /* Convert float FIR coefficients to Q15, applying x0.5 rate scaling
     * (mirrors the convention in hbdecim_core.c).  The first K entries of
     * h[] are the non-trivial coefficients; symmetry means h[K..num_taps-1]
     * are redundant.  We only store the first K values — the inner loop
     * exploits the mirror via the fold (a[k] + a[N-1-k]).                */
    for (size_t k = 0; k < r->K; k++) {
        double v = (double)h[k] * 0.5 * 32768.0;
        if (v >  32767.0) v =  32767.0;
        if (v < -32768.0) v = -32768.0;
        r->coeffs[k] = (int16_t)lround(v);
    }

    /* Dual-write ring capacity: next power-of-2 >= num_taps. */
    r->cap = 1;
    while (r->cap < num_taps)
        r->cap <<= 1;
    r->mask = r->cap - 1;

    r->even_I = calloc(2 * r->cap, sizeof(int16_t));
    r->even_Q = calloc(2 * r->cap, sizeof(int16_t));
    r->odd_I  = calloc(2 * r->cap, sizeof(int16_t));
    r->odd_Q  = calloc(2 * r->cap, sizeof(int16_t));
    if (!r->even_I || !r->even_Q || !r->odd_I || !r->odd_Q)
        goto fail;

    return r;

fail:
#ifdef _WIN32
    _aligned_free(r->coeffs);
#else
    free(r->coeffs);
#endif
    free(r->even_I);
    free(r->even_Q);
    free(r->odd_I);
    free(r->odd_Q);
    free(r);
    return NULL;
}

void
hbdecim_q15_destroy(hbdecim_q15_state_t *r)
{
    if (!r)
        return;
#ifdef _WIN32
    _aligned_free(r->coeffs);
#else
    free(r->coeffs);
#endif
    free(r->even_I);
    free(r->even_Q);
    free(r->odd_I);
    free(r->odd_Q);
    free(r);
}

void
hbdecim_q15_reset(hbdecim_q15_state_t *r)
{
    r->even_head  = 0;
    r->odd_head   = 0;
    r->has_pending = 0;
    memset(r->even_I, 0, 2 * r->cap * sizeof(int16_t));
    memset(r->even_Q, 0, 2 * r->cap * sizeof(int16_t));
    memset(r->odd_I,  0, 2 * r->cap * sizeof(int16_t));
    memset(r->odd_Q,  0, 2 * r->cap * sizeof(int16_t));
}

/* ================================================================== */
/* Properties                                                          */
/* ================================================================== */

size_t
hbdecim_q15_execute_max_out(hbdecim_q15_state_t *r)
{
    (void)r;
    /* Return 0 → Python glue sizes output to n_in (safe for 2:1 decim). */
    return 0;
}

double
hbdecim_q15_get_rate(const hbdecim_q15_state_t *r)
{
    (void)r;
    return 0.5;
}

size_t
hbdecim_q15_get_num_taps(const hbdecim_q15_state_t *r)
{
    return r->num_taps;
}

/* ================================================================== */
/* Execute                                                             */
/* ================================================================== */

size_t
hbdecim_q15_execute(hbdecim_q15_state_t *r,
                    const int16_t *in, size_t n_in,
                    int16_t *out, size_t max_out)
{
    if (!n_in || !max_out)
        return 0;

    size_t oi  = 0; /* output complex-sample index  */
    size_t xi  = 0; /* input  complex-sample index  */

    /* Complete a buffered even sample with the first odd sample. */
    if (r->has_pending && oi < max_out) {
        push_even(r, r->pending_I, r->pending_Q);
        push_odd(r, in[2 * xi], in[2 * xi + 1]);
        xi++;
        int16_t ri, rq;
        compute_output(r, &ri, &rq);
        out[2 * oi]     = ri;
        out[2 * oi + 1] = rq;
        oi++;
        r->has_pending = 0;
    }

    /* Process complete (even, odd) input pairs. */
    while (xi + 1 < n_in && oi < max_out) {
        push_even(r, in[2 * xi],     in[2 * xi + 1]);
        push_odd( r, in[2 * xi + 2], in[2 * xi + 3]);
        xi += 2;
        int16_t ri, rq;
        compute_output(r, &ri, &rq);
        out[2 * oi]     = ri;
        out[2 * oi + 1] = rq;
        oi++;
    }

    /* Buffer any trailing even sample. */
    if (xi < n_in) {
        r->pending_I   = in[2 * xi];
        r->pending_Q   = in[2 * xi + 1];
        r->has_pending = 1;
    }

    return oi;
}
