/**
 * @file dp_simd.h
 * @brief doppler's own composite SIMD reductions, layered over `jm_simd.h`.
 *
 * just-makeit ships the TIER primitives — `JM_VEC_F32`, `JM_LOAD_F32`,
 * `JM_FMA_F32`, `JM_HSUM_F32`, `JM_SIMD_WIDTH_F32` — one set per ISA
 * (AVX-512 / AVX2 / NEON / scalar). Anything doppler builds ON those belongs
 * here, in a header doppler owns.
 *
 * That distinction is the whole reason this file exists. `DP_SUMSQ_F32` lived
 * in `native/inc/jm_simd.h` from `bda43475` (the log-domain AGC) until it was
 * lost, and the way it was lost is worth recording: jm's headers are
 * **create-only**, so `jm apply` never rewrites them — which is exactly what
 * let a local addition survive there for months, and exactly why nothing
 * warned when the create-only migration (delete, re-apply, pick up the newer
 * upstream file) discarded it. A local extension inside a vendored file is
 * protected by nothing but the tool's reluctance to touch it.
 *
 * It also carried the wrong namespace. `JM_` belongs to just-makeit, and a
 * future release is free to define `JM_SUMSQ_F32` itself with different
 * semantics or arity; this is doppler's primitive, so it is `DP_`.
 */
#ifndef DP_SIMD_H
#define DP_SIMD_H

#include <stddef.h>

#include "jm_simd.h"

/**
 * @brief Sum of squares: dst = Σ ptr&#91;i&#93;² for i in &#91;0, n).
 *
 * The bulk runs JM_SIMD_WIDTH_F32-wide via FMA accumulation; the trailing
 * @c n % JM_SIMD_WIDTH_F32 elements are summed scalar. When @p n is a
 * multiple of the SIMD width the remainder loop has zero trips and folds
 * away, leaving a pure vector reduction.
 *
 * A macro rather than a function, and single-pointer rather than a dot
 * product against itself: `jm_dot_f32(a, a, n)` would be undefined behaviour,
 * because both of its parameters are `JM_RESTRICT` and passing one pointer to
 * two restrict-qualified parameters tells the compiler the buffers do not
 * overlap when they are the same buffer.
 *
 * @param dst  lvalue of type float — receives the sum.
 * @param ptr  const float * — base of the contiguous input.
 * @param n    element count (size_t-convertible).
 *
 * @code
 *   float e;
 *   DP_SUMSQ_F32 (e, buf, 256);   // e = energy of buf[0..255]
 * @endcode
 */
#define DP_SUMSQ_F32(dst, ptr, n)                                             \
  do                                                                          \
    {                                                                         \
      const float *dp_p_ = (ptr);                                             \
      size_t dp_n_ = (size_t)(n);                                             \
      size_t dp_nv_ = dp_n_ - dp_n_ % (size_t)JM_SIMD_WIDTH_F32;              \
      JM_VEC_F32 dp_acc_ = JM_ZERO_F32 ();                                    \
      for (size_t dp_i_ = 0; dp_i_ < dp_nv_;                                  \
           dp_i_ += (size_t)JM_SIMD_WIDTH_F32)                                \
        {                                                                     \
          JM_VEC_F32 dp_v_ = JM_LOAD_F32 (dp_p_ + dp_i_);                     \
          JM_FMA_F32 (dp_acc_, dp_v_, dp_v_);                                 \
        }                                                                     \
      float dp_s_ = JM_HSUM_F32 (dp_acc_);                                    \
      for (size_t dp_i_ = dp_nv_; dp_i_ < dp_n_; dp_i_++)                     \
        dp_s_ += dp_p_[dp_i_] * dp_p_[dp_i_];                                 \
      (dst) = dp_s_;                                                          \
    }                                                                         \
  while (0)

#endif /* DP_SIMD_H */
