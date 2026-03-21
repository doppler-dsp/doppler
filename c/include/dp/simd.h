/**
 * @file simd.h
 * @brief SIMD-accelerated complex arithmetic.
 *
 * On x86-64 with AVX2, operations use 256-bit vector instructions.
 * On AArch64 (ARM), the compiler emits equivalent NEON code from the
 * scalar C99 fallback.
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
 * On x86-64 with AVX2, uses 256-bit FMA instructions.  On other
 * architectures falls back to a plain C99 `a * b` (which the compiler
 * typically auto-vectorises to NEON on AArch64).
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
