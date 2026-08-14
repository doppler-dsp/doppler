/**
 * jm_simd.h — width-portable SIMD operation macros.
 *
 * Selects the widest available instruction set at compile time:
 *   AVX-512F  ->  16 float / 8 double lanes  (JM_SIMD_WIDTH_F32 = 16)
 *   AVX2+FMA  ->   8 float / 4 double lanes  (JM_SIMD_WIDTH_F32 =  8)
 *   NEON      ->   4 float / 2 double lanes  (JM_SIMD_WIDTH_F32 =  4)  (aarch64)
 *   Scalar    ->   1 lane  (auto-vectorisation still applies)
 *
 * Typical usage (FIR inner loop):
 *
 * @code
 *   JM_VEC_F32 acc = JM_ZERO_F32();
 *   for (int k = 0; k < N_TAPS; k++)
 *       JM_MAC_F32(acc, window + k, coeffs[k]);
 *   *out = JM_HSUM_F32(acc);
 * @endcode
 *
 * JM_SIMD_WIDTH_F32 tells you how many floats the loop above advances
 * per iteration — stride your outer loop accordingly.
 *
 * Can be included standalone; reuses JM_RESTRICT from jm_perf.h if
 * already defined, otherwise provides its own fallback.
 */
#ifndef JM_SIMD_H
#define JM_SIMD_H

/* Reuse JM_RESTRICT from jm_perf.h if available; otherwise define locally. */
#ifndef JM_RESTRICT
#  if defined(__GNUC__) || defined(__clang__)
#    define JM_RESTRICT __restrict__
#  elif defined(_MSC_VER)
#    define JM_RESTRICT __restrict
#  else
#    define JM_RESTRICT restrict
#  endif
#endif

/* Pull in x86 intrinsic headers if not already included. */
#if (defined(__x86_64__) || defined(_M_X64) || \
     defined(__i386__)   || defined(_M_IX86))
#  ifndef _IMMINTRIN_H_INCLUDED
#    include <immintrin.h>
#  endif
#endif

/* Pull in the NEON intrinsic header on aarch64 (mandatory baseline there,
 * unlike optional NEON on 32-bit ARM, which lacks double-precision support
 * and is out of scope). */
#if defined(__aarch64__)
#  include <arm_neon.h>
#endif

/* ════════════════════════════════════════════════════════════════════════
 * Tier 1 — AVX-512F  (16 float / 8 double lanes)
 * ════════════════════════════════════════════════════════════════════════ */
#if defined(__AVX512F__)

#define JM_SIMD_WIDTH_F32   16
#define JM_SIMD_WIDTH_F64    8
#define JM_SIMD_WIDTH       JM_SIMD_WIDTH_F32

typedef __m512  JM_VEC_F32;
typedef __m512d JM_VEC_F64;

#define JM_ZERO_F32()            _mm512_setzero_ps()
#define JM_ZERO_F64()            _mm512_setzero_pd()
#define JM_SPLAT_F32(x)          _mm512_set1_ps(x)
#define JM_SPLAT_F64(x)          _mm512_set1_pd(x)
#define JM_LOAD_F32(p)           _mm512_loadu_ps(p)
#define JM_LOAD_F64(p)           _mm512_loadu_pd(p)
#define JM_STORE_F32(p, v)       _mm512_storeu_ps(p, v)
#define JM_STORE_F64(p, v)       _mm512_storeu_pd(p, v)
#define JM_ADD_F32(a, b)         _mm512_add_ps(a, b)
#define JM_ADD_F64(a, b)         _mm512_add_pd(a, b)
#define JM_MUL_F32(a, b)         _mm512_mul_ps(a, b)
#define JM_MUL_F64(a, b)         _mm512_mul_pd(a, b)
/* acc += a * b */
#define JM_FMA_F32(acc, a, b)    ((acc) = _mm512_fmadd_ps(a, b, acc))
#define JM_FMA_F64(acc, a, b)    ((acc) = _mm512_fmadd_pd(a, b, acc))
/* Load JM_SIMD_WIDTH_F32 floats from ptr, multiply by scalar s, accumulate */
#define JM_MAC_F32(acc, ptr, s)  JM_FMA_F32(acc, JM_LOAD_F32(ptr), JM_SPLAT_F32(s))
#define JM_MAC_F64(acc, ptr, s)  JM_FMA_F64(acc, JM_LOAD_F64(ptr), JM_SPLAT_F64(s))
/* Horizontal sum: reduce all lanes to one scalar */
#define JM_HSUM_F32(v)           _mm512_reduce_add_ps(v)
#define JM_HSUM_F64(v)           _mm512_reduce_add_pd(v)

/* ════════════════════════════════════════════════════════════════════════
 * Tier 2 — AVX2 + FMA  (8 float / 4 double lanes)
 * ════════════════════════════════════════════════════════════════════════ */
#elif defined(__AVX2__) && defined(__FMA__)

#define JM_SIMD_WIDTH_F32    8
#define JM_SIMD_WIDTH_F64    4
#define JM_SIMD_WIDTH        JM_SIMD_WIDTH_F32

typedef __m256  JM_VEC_F32;
typedef __m256d JM_VEC_F64;

#define JM_ZERO_F32()            _mm256_setzero_ps()
#define JM_ZERO_F64()            _mm256_setzero_pd()
#define JM_SPLAT_F32(x)          _mm256_set1_ps(x)
#define JM_SPLAT_F64(x)          _mm256_set1_pd(x)
#define JM_LOAD_F32(p)           _mm256_loadu_ps(p)
#define JM_LOAD_F64(p)           _mm256_loadu_pd(p)
#define JM_STORE_F32(p, v)       _mm256_storeu_ps(p, v)
#define JM_STORE_F64(p, v)       _mm256_storeu_pd(p, v)
#define JM_ADD_F32(a, b)         _mm256_add_ps(a, b)
#define JM_ADD_F64(a, b)         _mm256_add_pd(a, b)
#define JM_MUL_F32(a, b)         _mm256_mul_ps(a, b)
#define JM_MUL_F64(a, b)         _mm256_mul_pd(a, b)
#define JM_FMA_F32(acc, a, b)    ((acc) = _mm256_fmadd_ps(a, b, acc))
#define JM_FMA_F64(acc, a, b)    ((acc) = _mm256_fmadd_pd(a, b, acc))
#define JM_MAC_F32(acc, ptr, s)  JM_FMA_F32(acc, JM_LOAD_F32(ptr), JM_SPLAT_F32(s))
#define JM_MAC_F64(acc, ptr, s)  JM_FMA_F64(acc, JM_LOAD_F64(ptr), JM_SPLAT_F64(s))

/* Horizontal-sum helpers (SSE3 hadd guaranteed when AVX2 is available) */
static inline float jm_hsum256_f32(__m256 v) {
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 hi = _mm256_extractf128_ps(v, 1);
    __m128 s  = _mm_add_ps(lo, hi);
    s = _mm_hadd_ps(s, s);
    s = _mm_hadd_ps(s, s);
    return _mm_cvtss_f32(s);
}
static inline double jm_hsum256_f64(__m256d v) {
    __m128d lo = _mm256_castpd256_pd128(v);
    __m128d hi = _mm256_extractf128_pd(v, 1);
    __m128d s  = _mm_add_pd(lo, hi);
    s = _mm_hadd_pd(s, s);
    return _mm_cvtsd_f64(s);
}
#define JM_HSUM_F32(v)           jm_hsum256_f32(v)
#define JM_HSUM_F64(v)           jm_hsum256_f64(v)

/* ════════════════════════════════════════════════════════════════════════
 * Tier 3 — NEON  (4 float / 2 double lanes; aarch64 only — 32-bit ARM NEON
 * has no double-precision support, so this is gated on __aarch64__, not
 * the broader __ARM_NEON, which is also defined there)
 * ════════════════════════════════════════════════════════════════════════ */
#elif defined(__aarch64__)

#define JM_SIMD_WIDTH_F32    4
#define JM_SIMD_WIDTH_F64    2
#define JM_SIMD_WIDTH        JM_SIMD_WIDTH_F32

typedef float32x4_t JM_VEC_F32;
typedef float64x2_t JM_VEC_F64;

#define JM_ZERO_F32()            vdupq_n_f32(0.0f)
#define JM_ZERO_F64()            vdupq_n_f64(0.0)
#define JM_SPLAT_F32(x)          vdupq_n_f32(x)
#define JM_SPLAT_F64(x)          vdupq_n_f64(x)
#define JM_LOAD_F32(p)           vld1q_f32(p)
#define JM_LOAD_F64(p)           vld1q_f64(p)
#define JM_STORE_F32(p, v)       vst1q_f32(p, v)
#define JM_STORE_F64(p, v)       vst1q_f64(p, v)
#define JM_ADD_F32(a, b)         vaddq_f32(a, b)
#define JM_ADD_F64(a, b)         vaddq_f64(a, b)
#define JM_MUL_F32(a, b)         vmulq_f32(a, b)
#define JM_MUL_F64(a, b)         vmulq_f64(a, b)
/* acc += a * b (vfmaq_f32(a,b,c) computes a + b*c, so acc = vfmaq_f32(acc,a,b)) */
#define JM_FMA_F32(acc, a, b)    ((acc) = vfmaq_f32(acc, a, b))
#define JM_FMA_F64(acc, a, b)    ((acc) = vfmaq_f64(acc, a, b))
#define JM_MAC_F32(acc, ptr, s)  JM_FMA_F32(acc, JM_LOAD_F32(ptr), JM_SPLAT_F32(s))
#define JM_MAC_F64(acc, ptr, s)  JM_FMA_F64(acc, JM_LOAD_F64(ptr), JM_SPLAT_F64(s))
/* ARMv8-A provides a direct across-vector horizontal add; no manual hadd needed */
#define JM_HSUM_F32(v)           vaddvq_f32(v)
#define JM_HSUM_F64(v)           vaddvq_f64(v)

/* ════════════════════════════════════════════════════════════════════════
 * Tier 4 — Scalar  (1 lane; compiler auto-vectorisation still applies)
 * ════════════════════════════════════════════════════════════════════════ */
#else

#define JM_SIMD_WIDTH_F32    1
#define JM_SIMD_WIDTH_F64    1
#define JM_SIMD_WIDTH        1

typedef float  JM_VEC_F32;
typedef double JM_VEC_F64;

#define JM_ZERO_F32()            0.0f
#define JM_ZERO_F64()            0.0
#define JM_SPLAT_F32(x)          (x)
#define JM_SPLAT_F64(x)          (x)
#define JM_LOAD_F32(p)           (*(p))
#define JM_LOAD_F64(p)           (*(p))
#define JM_STORE_F32(p, v)       (*(p) = (v))
#define JM_STORE_F64(p, v)       (*(p) = (v))
#define JM_ADD_F32(a, b)         ((a) + (b))
#define JM_ADD_F64(a, b)         ((a) + (b))
#define JM_MUL_F32(a, b)         ((a) * (b))
#define JM_MUL_F64(a, b)         ((a) * (b))
#define JM_FMA_F32(acc, a, b)    ((acc) += (a) * (b))
#define JM_FMA_F64(acc, a, b)    ((acc) += (a) * (b))
#define JM_MAC_F32(acc, ptr, s)  ((acc) += (*(ptr)) * (s))
#define JM_MAC_F64(acc, ptr, s)  ((acc) += (*(ptr)) * (s))
#define JM_HSUM_F32(v)           (v)
#define JM_HSUM_F64(v)           (v)

#endif /* SIMD tier */

/* ── Dot product: SIMD-vectorised + scalar tail ───────────────────────── */

static inline float jm_dot_f32(
        const float  * JM_RESTRICT a,
        const float  * JM_RESTRICT b, int n)
{
    JM_VEC_F32 acc = JM_ZERO_F32();
    int i = 0;
#if JM_SIMD_WIDTH_F32 > 1
    for (; i <= n - JM_SIMD_WIDTH_F32; i += JM_SIMD_WIDTH_F32)
        JM_FMA_F32(acc, JM_LOAD_F32(a + i), JM_LOAD_F32(b + i));
#endif
    float s = JM_HSUM_F32(acc);
    for (; i < n; i++) s += a[i] * b[i];
    return s;
}

static inline double jm_dot_f64(
        const double * JM_RESTRICT a,
        const double * JM_RESTRICT b, int n)
{
    JM_VEC_F64 acc = JM_ZERO_F64();
    int i = 0;
#if JM_SIMD_WIDTH_F64 > 1
    for (; i <= n - JM_SIMD_WIDTH_F64; i += JM_SIMD_WIDTH_F64)
        JM_FMA_F64(acc, JM_LOAD_F64(a + i), JM_LOAD_F64(b + i));
#endif
    double s = JM_HSUM_F64(acc);
    for (; i < n; i++) s += a[i] * b[i];
    return s;
}

#endif /* JM_SIMD_H */
