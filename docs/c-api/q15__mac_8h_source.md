

# File q15\_mac.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**q15\_mac.h**](q15__mac_8h.md)

[Go to the documentation of this file](q15__mac_8h.md)


```C++

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

static inline int64_t
dot_q15_scalar(const int16_t *a, const int16_t *b, size_t n)
{
    int64_t acc = 0;
    for (size_t k = 0; k < n; k++)
        acc += (int64_t)a[k] * b[k];
    return acc;
}

#endif /* Q15_MAC_H */
```


