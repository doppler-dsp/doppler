

# File jm\_simd.h

[**File List**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**jm\_simd.h**](jm__simd_8h.md)

[Go to the documentation of this file](jm__simd_8h.md)


```C++


#ifndef JM_SIMD_H
#define JM_SIMD_H

/* Reuse JM_RESTRICT from jm_perf.h if available; otherwise define. */
#ifndef JM_RESTRICT
#  if defined(__GNUC__) || defined(__clang__)
#    define JM_RESTRICT __restrict__
#  elif defined(_MSC_VER)
#    define JM_RESTRICT __restrict
#  else
#    define JM_RESTRICT restrict
#  endif
#endif

/* Pull in x86 intrinsic headers. */
#if (defined(__x86_64__) || defined(_M_X64) || \
     defined(__i386__)   || defined(_M_IX86))
#  ifndef _IMMINTRIN_H_INCLUDED
#    include <immintrin.h>
#  endif
#endif

/* ════════════════════════════════════════════════════════════════════
 * Tier 1 — AVX-512F  (16 float / 8 double lanes)
 * ════════════════════════════════════════════════════════════════════ */
#if defined(__AVX512F__)

#define JM_SIMD_WIDTH_F32  16
#define JM_SIMD_WIDTH_F64   8
#define JM_SIMD_WIDTH      JM_SIMD_WIDTH_F32

typedef __m512  JM_VEC_F32;
typedef __m512d JM_VEC_F64;

#define JM_ZERO_F32()           _mm512_setzero_ps()
#define JM_ZERO_F64()           _mm512_setzero_pd()
#define JM_SPLAT_F32(x)         _mm512_set1_ps(x)
#define JM_SPLAT_F64(x)         _mm512_set1_pd(x)
#define JM_LOAD_F32(p)          _mm512_loadu_ps(p)
#define JM_LOAD_F64(p)          _mm512_loadu_pd(p)
#define JM_STORE_F32(p, v)      _mm512_storeu_ps(p, v)
#define JM_STORE_F64(p, v)      _mm512_storeu_pd(p, v)
#define JM_ADD_F32(a, b)        _mm512_add_ps(a, b)
#define JM_ADD_F64(a, b)        _mm512_add_pd(a, b)
#define JM_MUL_F32(a, b)        _mm512_mul_ps(a, b)
#define JM_MUL_F64(a, b)        _mm512_mul_pd(a, b)
#define JM_FMA_F32(acc, a, b)   ((acc) = _mm512_fmadd_ps(a, b, acc))
#define JM_FMA_F64(acc, a, b)   ((acc) = _mm512_fmadd_pd(a, b, acc))
#define JM_MAC_F32(acc, ptr, s) \
        JM_FMA_F32(acc, JM_LOAD_F32(ptr), JM_SPLAT_F32(s))
#define JM_MAC_F64(acc, ptr, s) \
        JM_FMA_F64(acc, JM_LOAD_F64(ptr), JM_SPLAT_F64(s))
#define JM_HSUM_F32(v)          _mm512_reduce_add_ps(v)
#define JM_HSUM_F64(v)          _mm512_reduce_add_pd(v)

/* ════════════════════════════════════════════════════════════════════
 * Tier 2 — AVX2 + FMA  (8 float / 4 double lanes)
 * ════════════════════════════════════════════════════════════════════ */
#elif defined(__AVX2__) && defined(__FMA__)

#define JM_SIMD_WIDTH_F32   8
#define JM_SIMD_WIDTH_F64   4
#define JM_SIMD_WIDTH       JM_SIMD_WIDTH_F32

typedef __m256  JM_VEC_F32;
typedef __m256d JM_VEC_F64;

#define JM_ZERO_F32()           _mm256_setzero_ps()
#define JM_ZERO_F64()           _mm256_setzero_pd()
#define JM_SPLAT_F32(x)         _mm256_set1_ps(x)
#define JM_SPLAT_F64(x)         _mm256_set1_pd(x)
#define JM_LOAD_F32(p)          _mm256_loadu_ps(p)
#define JM_LOAD_F64(p)          _mm256_loadu_pd(p)
#define JM_STORE_F32(p, v)      _mm256_storeu_ps(p, v)
#define JM_STORE_F64(p, v)      _mm256_storeu_pd(p, v)
#define JM_ADD_F32(a, b)        _mm256_add_ps(a, b)
#define JM_ADD_F64(a, b)        _mm256_add_pd(a, b)
#define JM_MUL_F32(a, b)        _mm256_mul_ps(a, b)
#define JM_MUL_F64(a, b)        _mm256_mul_pd(a, b)
#define JM_FMA_F32(acc, a, b)   ((acc) = _mm256_fmadd_ps(a, b, acc))
#define JM_FMA_F64(acc, a, b)   ((acc) = _mm256_fmadd_pd(a, b, acc))
#define JM_MAC_F32(acc, ptr, s) \
        JM_FMA_F32(acc, JM_LOAD_F32(ptr), JM_SPLAT_F32(s))
#define JM_MAC_F64(acc, ptr, s) \
        JM_FMA_F64(acc, JM_LOAD_F64(ptr), JM_SPLAT_F64(s))

/* Horizontal-sum helpers (SSE3 hadd guaranteed with AVX2). */
static inline float _jm_hsum256_f32 (__m256 v)
{
  __m128 lo = _mm256_castps256_ps128 (v);
  __m128 hi = _mm256_extractf128_ps (v, 1);
  __m128 s  = _mm_add_ps (lo, hi);
  s = _mm_hadd_ps (s, s);
  s = _mm_hadd_ps (s, s);
  return _mm_cvtss_f32 (s);
}
static inline double _jm_hsum256_f64 (__m256d v)
{
  __m128d lo = _mm256_castpd256_pd128 (v);
  __m128d hi = _mm256_extractf128_pd (v, 1);
  __m128d s  = _mm_add_pd (lo, hi);
  s = _mm_hadd_pd (s, s);
  return _mm_cvtsd_f64 (s);
}
#define JM_HSUM_F32(v)  _jm_hsum256_f32(v)
#define JM_HSUM_F64(v)  _jm_hsum256_f64(v)

/* ════════════════════════════════════════════════════════════════════
 * Tier 3 — Scalar  (1 lane; auto-vectorisation still applies)
 * ════════════════════════════════════════════════════════════════════ */
#else

#define JM_SIMD_WIDTH_F32   1
#define JM_SIMD_WIDTH_F64   1
#define JM_SIMD_WIDTH       1

typedef float  JM_VEC_F32;
typedef double JM_VEC_F64;

#define JM_ZERO_F32()           (0.0f)
#define JM_ZERO_F64()           (0.0)
#define JM_SPLAT_F32(x)         ((float)(x))
#define JM_SPLAT_F64(x)         ((double)(x))
#define JM_LOAD_F32(p)          (*(const float *)(p))
#define JM_LOAD_F64(p)          (*(const double *)(p))
#define JM_STORE_F32(p, v)      (*(float *)(p) = (v))
#define JM_STORE_F64(p, v)      (*(double *)(p) = (v))
#define JM_ADD_F32(a, b)        ((a) + (b))
#define JM_ADD_F64(a, b)        ((a) + (b))
#define JM_MUL_F32(a, b)        ((a) * (b))
#define JM_MUL_F64(a, b)        ((a) * (b))
#define JM_FMA_F32(acc, a, b)   ((acc) += (a) * (b))
#define JM_FMA_F64(acc, a, b)   ((acc) += (a) * (b))
#define JM_MAC_F32(acc, ptr, s) \
        ((acc) += (*(const float *)(ptr)) * (float)(s))
#define JM_MAC_F64(acc, ptr, s) \
        ((acc) += (*(const double *)(ptr)) * (double)(s))
#define JM_HSUM_F32(v)          ((float)(v))
#define JM_HSUM_F64(v)          ((double)(v))

#endif /* ISA tiers */

#endif /* JM_SIMD_H */
```
