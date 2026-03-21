#include "dp/simd.h"
#include <complex.h>

/*
 * dp_c16_mul — complex128 multiply.
 *
 * On x86/x86-64 we use SSE2 (two double registers, no loop overhead).
 * On every other architecture (ARM, RISC-V, …) we fall back to the
 * C99 _Complex multiply, which the compiler optimises well.
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

#else /* ARM, Apple Silicon, RISC-V, WASM, … */

double complex
dp_c16_mul (double complex a, double complex b)
{
  /* C99 _Complex arithmetic — compiler emits FMUL/FADD pairs on NEON. */
  return a * b;
}

#endif
