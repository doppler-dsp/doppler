#include "dp/util.h"
#include <complex.h>

/*
 * dp_c16_mul — complex128 multiply.
 *
 * Three compile-time paths:
 *   x86 / x86-64  — SSE2 128-bit intrinsics (__m128d)
 *   AArch64        — NEON 128-bit intrinsics (float64x2_t)
 *   Other          — C99 _Complex fallback (scalar / compiler auto-vec)
 */

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)               \
    || defined(_M_IX86)

#include <immintrin.h> /* SSE2 is baseline for all x86-64 targets */

double complex
dp_c16_mul (double complex a, double complex b)
{
  __m128d va = _mm_loadu_pd ((const double *)&a); /* [re(a), im(a)] */
  __m128d vb = _mm_loadu_pd ((const double *)&b); /* [re(b), im(b)] */

  /*
   * Standard SSE2 complex multiply:
   *   re(a*b) = re(a)*re(b) - im(a)*im(b)  = ac - bd
   *   im(a*b) = re(a)*im(b) + im(a)*re(b)  = ad + bc
   *
   * Step 1: broadcast re(a) and im(a) into separate registers.
   * Step 2: multiply by [re(b), im(b)] and [im(b), re(b)] respectively.
   *         term1 = [ac, ad],  term2 = [bd, bc]
   * Step 3: negate lane 0 of term2: [-bd, +bc]
   * Step 4: add: [ac-bd, ad+bc]
   */
  __m128d va_rr = _mm_unpacklo_pd (va, va);     /* [re(a), re(a)] */
  __m128d va_ii = _mm_unpackhi_pd (va, va);     /* [im(a), im(a)] */
  __m128d vb_ir = _mm_shuffle_pd (vb, vb, 0x1); /* [im(b), re(b)] */

  __m128d term1 = _mm_mul_pd (va_rr, vb);    /* [ac, ad] */
  __m128d term2 = _mm_mul_pd (va_ii, vb_ir); /* [bd, bc] */

  /* _mm_set_pd(hi, lo) — negate lo (lane 0 = real part = bd) */
  __m128d neg = _mm_set_pd (1.0, -1.0);
  term2 = _mm_mul_pd (term2, neg); /* [-bd, +bc] */

  __m128d res = _mm_add_pd (term1, term2); /* [ac-bd, ad+bc] */

  double complex out;
  _mm_storeu_pd ((double *)&out, res);
  return out;
}

#elif defined(__aarch64__) || defined(_M_ARM64)

#include <arm_neon.h>

double complex
dp_c16_mul (double complex a, double complex b)
{
  float64x2_t va = vld1q_f64 ((const double *)&a); /* [re(a), im(a)] */
  float64x2_t vb = vld1q_f64 ((const double *)&b); /* [re(b), im(b)] */

  /*
   * NEON complex multiply (mirrors the SSE2 path above):
   *   re(a*b) = re(a)*re(b) - im(a)*im(b)  = ac - bd
   *   im(a*b) = re(a)*im(b) + im(a)*re(b)  = ad + bc
   *
   * Step 1: broadcast re(a) and im(a) into separate registers.
   * Step 2: swap lanes of b → [im(b), re(b)].
   * Step 3: term1 = [ac, ad],  term2 = [bd, bc].
   * Step 4: negate lane 0 of term2 → [-bd, +bc].
   * Step 5: add → [ac-bd, ad+bc].
   */
  float64x2_t va_rr = vdupq_laneq_f64 (va, 0); /* [re(a), re(a)] */
  float64x2_t va_ii = vdupq_laneq_f64 (va, 1); /* [im(a), im(a)] */
  float64x2_t vb_ir = vextq_f64 (vb, vb, 1);   /* [im(b), re(b)] */

  float64x2_t term1 = vmulq_f64 (va_rr, vb);    /* [ac, ad] */
  float64x2_t term2 = vmulq_f64 (va_ii, vb_ir); /* [bd, bc] */

  /* negate lane 0 only → [-bd, +bc] */
  static const double sign_arr[2] = { -1.0, 1.0 };
  float64x2_t sign = vld1q_f64 (sign_arr);
  term2 = vmulq_f64 (term2, sign);

  float64x2_t res = vaddq_f64 (term1, term2); /* [ac-bd, ad+bc] */

  double complex out;
  vst1q_f64 ((double *)&out, res);
  return out;
}

#else /* RISC-V, WASM, and other architectures */

double complex
dp_c16_mul (double complex a, double complex b)
{
  /* C99 _Complex arithmetic — best-effort scalar/auto-vec fallback. */
  return a * b;
}

#endif
