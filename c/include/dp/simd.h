/**
 * @file simd.h
 * @brief SIMD-accelerated complex arithmetic.
 *
 * Dispatch is compile-time by architecture:
 *   - x86 / x86-64: SSE2 128-bit intrinsics (baseline for all x86-64)
 *   - AArch64 (ARM64): NEON 128-bit float64x2 intrinsics
 *   - Other: C99 `a * b` scalar fallback
 *
 * ```c
 * #include <dp/simd.h>
 * #include <complex.h>
 *
 * double complex a = 1.0 + 2.0*I;
 * double complex b = 3.0 + 4.0*I;
 * double complex c = dp_c16_mul(a, b);  // (1+2i)(3+4i) = -5+10i
 * ```
 */

#ifndef DP_SIMD_H
#define DP_SIMD_H

#include <complex.h>

/**
 * @brief Multiply two complex doubles using the fastest available SIMD path.
 *
 * Uses SSE2 on x86/x86-64, NEON float64x2 intrinsics on AArch64, and
 * a plain C99 `a * b` scalar fallback on all other architectures.
 *
 * @param a First complex operand.
 * @param b Second complex operand.
 * @return  @p a × @p b.
 */
#ifdef __GNUC__
__attribute__ ((const))
#endif
double complex dp_c16_mul (double complex a, double complex b);

#endif /* DP_SIMD_H */
