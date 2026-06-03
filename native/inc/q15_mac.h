/**
 * @file q15_mac.h
 * @brief Static inline Q15 dot-product primitives: scalar fallback and AVX2.
 *
 * Provides two functions:
 *
 *   int64_t dot_q15_scalar(a, b, n)
 *   int64_t dot_q15_avx2 (a, b, n)  — only when __AVX2__ is defined
 *
 * Both compute the exact integer inner product sum(a[i] * b[i]) for i in
 * [0, n), accumulating into int64_t without saturation.  The caller decides
 * how to round and saturate the result.
 *
 * Result format: Q30 (each Q15 × Q15 product is Q30; n products accumulate
 * in int64_t, which has sufficient headroom for n ≤ 2^33 before overflow).
 *
 * The hsum_epi32_i64 helper is also exposed so callers that maintain their
 * own AVX2 accumulator can reduce it to a scalar at the end.
 *
 * Usage:
 * @code
 *   #include "q15_mac.h"
 *
 *   int64_t acc;
 *   #if defined(__AVX2__)
 *       acc = dot_q15_avx2(a, b, n);
 *   #else
 *       acc = dot_q15_scalar(a, b, n);
 *   #endif
 *   // shift to Q15: int32_t out = (int32_t)((acc + (1 << 14)) >> 15);
 * @endcode
 */
#ifndef Q15_MAC_H
#define Q15_MAC_H

#include <stddef.h>
#include <stdint.h>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
#  include <immintrin.h>
#endif

/* ================================================================== */
/* AVX2 helpers                                                        */
/* ================================================================== */

#if defined(__AVX2__)

/**
 * @brief Reduce 8 int32_t AVX2 lanes to a single int64_t scalar.
 *
 * Sign-extends every int32 lane to int64 before horizontal reduction to
 * avoid overflow when lanes approach INT32_MAX.  This matters for general
 * Q15 dot products where each madd lane can reach 2 × 32767² = 2,147,352,578
 * — adding two such lanes with int32 arithmetic would overflow.
 */
static inline int64_t
hsum_epi32_i64(__m256i v)
{
    __m128i lo128  = _mm256_extracti128_si256(v, 0);
    __m128i hi128  = _mm256_extracti128_si256(v, 1);
    /* Sign-extend all 8 int32 lanes to 8 × int64 (two 256-bit vectors). */
    __m256i lo64   = _mm256_cvtepi32_epi64(lo128);
    __m256i hi64   = _mm256_cvtepi32_epi64(hi128);
    __m256i sum256 = _mm256_add_epi64(lo64, hi64);   /* 4 × int64           */
    __m128i slo    = _mm256_extracti128_si256(sum256, 0);
    __m128i shi    = _mm256_extracti128_si256(sum256, 1);
    __m128i r      = _mm_add_epi64(slo, shi);         /* 2 × int64           */
    return (int64_t)_mm_extract_epi64(r, 0) + (int64_t)_mm_extract_epi64(r, 1);
}

/**
 * @brief Vectorised Q15 dot product using _mm256_madd_epi16.
 *
 * Processes 16 pairs per iteration.  A scalar tail handles the remainder
 * when n is not a multiple of 16.
 *
 * _mm256_madd_epi16 multiplies adjacent int16 pairs and accumulates into
 * int32 lanes: acc_i32[j] += a[2j]*b[2j] + a[2j+1]*b[2j+1].  Each lane
 * stays within int32_t provided |a[k]| and |b[k]| ≤ 32767 (always true
 * for Q15), so no intermediate overflow occurs.
 *
 * @param a  Q15 samples, not required to be aligned.
 * @param b  Q15 coefficients, not required to be aligned.
 * @param n  Number of elements (any value; tail handled scalar).
 * @return   Exact inner product in Q30 as int64_t.
 */
static int64_t
dot_q15_avx2(const int16_t *a, const int16_t *b, size_t n)
{
    int64_t result = 0;
    size_t  k      = 0;

    /* Flush to int64 after every 16-element block.
     *
     * A single _mm256_madd_epi16 call over all-Q15_MAX inputs produces
     * int32 lanes of 2 × 32767² = 2,147,352,578 — only 131,069 below
     * INT32_MAX.  Accumulating two such calls overflows int32.  We therefore
     * reduce each block to int64 before moving on; the scalar tail handles
     * any remainder.                                                          */
    for (; k + 16 <= n; k += 16) {
        __m256i va   = _mm256_loadu_si256((const __m256i *)(a + k));
        __m256i vb   = _mm256_loadu_si256((const __m256i *)(b + k));
        result      += hsum_epi32_i64(_mm256_madd_epi16(va, vb));
    }

    for (; k < n; k++)
        result += (int64_t)a[k] * b[k];

    return result;
}

#endif /* __AVX2__ */

/* ================================================================== */
/* Scalar fallback                                                     */
/* ================================================================== */

/**
 * @brief Scalar Q15 dot product.
 *
 * Each product is widened to int32_t before accumulation into int64_t to
 * avoid overflow.  Correct for all Q15 inputs and any n ≤ 2^33.
 */
static inline int64_t
dot_q15_scalar(const int16_t *a, const int16_t *b, size_t n)
{
    int64_t acc = 0;
    for (size_t k = 0; k < n; k++)
        acc += (int64_t)a[k] * b[k];
    return acc;
}

#endif /* Q15_MAC_H */
