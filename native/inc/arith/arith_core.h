/**
 * @file arith_core.h
 * @brief Arith module — public C API for fixed-point arithmetic on Q15
 * (int16_t) and Q8 (int8_t) arrays. All elementwise operations write into
 * a caller-supplied output buffer of the same length as the shorter input.
 * Saturation clamps results to the representable range rather than wrapping,
 * matching the two's-complement DSP convention.
 */
#ifndef ARITH_CORE_H
#define ARITH_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Saturation helpers — two's complement, matching project convention. */

/* Clamp a 32-bit intermediate to the signed 16-bit (Q15) range. */
static inline int16_t sat16(int32_t x)
{
    return x > 32767 ? 32767 : x < -32768 ? -32768 : (int16_t)x;
}

/* Clamp a 16-bit intermediate to the signed 8-bit (Q8) range. */
static inline int8_t sat8(int16_t x)
{
    return x > 127 ? 127 : x < -128 ? -128 : (int8_t)x;
}

/**
 * @brief Elementwise saturating add of two Q15 arrays.
 * Each pair of samples is added as int32_t and then clamped to [-32768,
 * 32767] before storing, so overflow wraps to the saturation boundary
 * rather than producing garbage bits.
 *
 * @param a      First input array (int16_t).
 * @param a_len  Number of elements in a.
 * @param b      Second input array (int16_t), same length as a.
 * @param b_len  Number of elements in b.
 * @param out    Output array (int16_t), length min(a_len, b_len).
 *
 * @code
 * >>> from doppler.arith import add_q15
 * >>> import numpy as np
 * >>> a = np.array([100, 20000, -20000], dtype=np.int16)
 * >>> b = np.array([50,  20000, -20000], dtype=np.int16)
 * >>> add_q15(a, b).tolist()
 * [150, 32767, -32768]
 * @endcode
 */
void add_q15(const int16_t *a, size_t a_len, const int16_t *b, size_t b_len, int16_t *out);


/**
 * @brief Elementwise saturating subtract of two Q15 arrays.
 * Computes out[i] = sat16(a[i] - b[i]) for i in [0, min(len(a), len(b))).
 * The intermediate difference is computed as int32_t to detect overflow
 * before clamping, preserving the correct sign at the saturation boundary.
 *
 * @param a      Minuend array (int16_t).
 * @param a_len  Number of elements in a.
 * @param b      Subtrahend array (int16_t), same length as a.
 * @param b_len  Number of elements in b.
 * @param out    Output array (int16_t), length min(a_len, b_len).
 *
 * @code
 * >>> from doppler.arith import sub_q15
 * >>> import numpy as np
 * >>> a = np.array([100,  0, -32768], dtype=np.int16)
 * >>> b = np.array([50,   0,     10], dtype=np.int16)
 * >>> sub_q15(a, b).tolist()
 * [50, 0, -32768]
 * @endcode
 */
void sub_q15(const int16_t *a, size_t a_len, const int16_t *b, size_t b_len, int16_t *out);


/**
 * @brief Elementwise Q15 multiply with round-half-up and saturation.
 * Computes out[i] = sat16((a[i] * b[i] + 16384) >> 15). The bias 16384
 * (= 1 << 14) implements round-half-up before the 15-bit right shift, so
 * 0.5 * 0.5 = 0.25 in Q15 arithmetic (16384 * 16384 -> 8192) rather than
 * truncating toward zero.
 *
 * @param a      First input array (int16_t).
 * @param a_len  Number of elements in a.
 * @param b      Second input array (int16_t), same length as a.
 * @param b_len  Number of elements in b.
 * @param out    Output array (int16_t), length min(a_len, b_len).
 *
 * @code
 * >>> from doppler.arith import mul_q15
 * >>> import numpy as np
 * >>> a = np.array([16384, 16384, 32767], dtype=np.int16)
 * >>> b = np.array([16384, -16384, 32767], dtype=np.int16)
 * >>> mul_q15(a, b).tolist()
 * [8192, -8192, 32766]
 * @endcode
 */
void mul_q15(const int16_t *a, size_t a_len, const int16_t *b, size_t b_len, int16_t *out);


/**
 * @brief Inner product of two Q15 arrays, returning the raw Q30 sum.
 * Accumulates products as int64_t — no scaling or saturation is applied
 * here. The caller shifts the result right 15 bits (e.g. via shr_i64)
 * to recover a Q15 scalar, or keeps the Q30 value for further arithmetic.
 *
 * @param a      First input array (int16_t).
 * @param a_len  Number of elements in a.
 * @param b      Second input array (int16_t), same length as a.
 * @param b_len  Number of elements in b.
 * @return Raw Q30 accumulation (int64_t).
 *
 * @code
 * >>> from doppler.arith import dot_q15
 * >>> import numpy as np
 * >>> a = np.array([100, 200, 300], dtype=np.int16)
 * >>> b = np.array([1, 2, 3], dtype=np.int16)
 * >>> dot_q15(a, b)
 * 1400
 * @endcode
 */
int64_t dot_q15(const int16_t *a, size_t a_len, const int16_t *b, size_t b_len);


/**
 * @brief Elementwise arithmetic left shift of a Q15 array with saturation.
 * Computes out[i] = sat16(a[i] << n), equivalent to multiplying by 2^n in
 * fixed-point. Any shifted value that exceeds the int16_t range is clamped,
 * preventing silent wraparound.
 *
 * @param a      Input array (int16_t).
 * @param a_len  Number of elements in a.
 * @param out    Output array (int16_t), same length as a.
 * @param n      Shift count (non-negative integer).
 *
 * @code
 * >>> from doppler.arith import shl_q15
 * >>> import numpy as np
 * >>> a = np.array([8192, 16384, 20000], dtype=np.int16)
 * >>> shl_q15(a, 1).tolist()
 * [16384, 32767, 32767]
 * @endcode
 */
void shl_q15(const int16_t *a, size_t a_len, int16_t *out, int n);


/**
 * @brief Elementwise arithmetic right shift of a Q15 array with
 * round-half-up. Adds 1 << (n-1) as a rounding bias before shifting, so
 * values exactly at the half-LSB boundary round up rather than truncating.
 * Equivalent to dividing by 2^n in Q15 fixed-point with correct rounding.
 *
 * @param a      Input array (int16_t).
 * @param a_len  Number of elements in a.
 * @param out    Output array (int16_t), same length as a.
 * @param n      Shift count (non-negative integer).
 *
 * @code
 * >>> from doppler.arith import shr_q15
 * >>> import numpy as np
 * >>> a = np.array([100, 101, 102, -100], dtype=np.int16)
 * >>> shr_q15(a, 2).tolist()
 * [25, 25, 26, -25]
 * @endcode
 */
void shr_q15(const int16_t *a, size_t a_len, int16_t *out, int n);


/**
 * @brief Elementwise saturating add of two Q8 arrays.
 * Each pair of samples is added as int16_t and then clamped to [-128, 127]
 * before storing, preventing overflow wrap on near-boundary values.
 *
 * @param a      First input array (int8_t).
 * @param a_len  Number of elements in a.
 * @param b      Second input array (int8_t), same length as a.
 * @param b_len  Number of elements in b.
 * @param out    Output array (int8_t), length min(a_len, b_len).
 *
 * @code
 * >>> from doppler.arith import add_q8
 * >>> import numpy as np
 * >>> a = np.array([50, 100, -100], dtype=np.int8)
 * >>> b = np.array([50,  30,  -50], dtype=np.int8)
 * >>> add_q8(a, b).tolist()
 * [100, 127, -128]
 * @endcode
 */
void add_q8(const int8_t *a, size_t a_len, const int8_t *b, size_t b_len, int8_t *out);


/**
 * @brief Elementwise saturating subtract of two Q8 arrays.
 * Computes out[i] = sat8(a[i] - b[i]) for i in [0, min(len(a), len(b))).
 * The intermediate difference is computed as int16_t so overflow is
 * detected before clamping.
 *
 * @param a      Minuend array (int8_t).
 * @param a_len  Number of elements in a.
 * @param b      Subtrahend array (int8_t), same length as a.
 * @param b_len  Number of elements in b.
 * @param out    Output array (int8_t), length min(a_len, b_len).
 *
 * @code
 * >>> from doppler.arith import sub_q8
 * >>> import numpy as np
 * >>> a = np.array([50,   0, -128], dtype=np.int8)
 * >>> b = np.array([30,   0,   10], dtype=np.int8)
 * >>> sub_q8(a, b).tolist()
 * [20, 0, -128]
 * @endcode
 */
void sub_q8(const int8_t *a, size_t a_len, const int8_t *b, size_t b_len, int8_t *out);


/**
 * @brief Elementwise Q8 multiply with round-half-up and saturation.
 * Computes out[i] = sat8((a[i] * b[i] + 64) >> 7). The bias 64 (= 1 << 6)
 * rounds at the half-LSB position before the 7-bit shift, mirroring the
 * rounding convention of mul_q15 but for 8-bit fixed-point. In Q8 terms,
 * 0.5 * 0.5 = 0.25 (64 * 64 -> 32).
 *
 * @param a      First input array (int8_t).
 * @param a_len  Number of elements in a.
 * @param b      Second input array (int8_t), same length as a.
 * @param b_len  Number of elements in b.
 * @param out    Output array (int8_t), length min(a_len, b_len).
 *
 * @code
 * >>> from doppler.arith import mul_q8
 * >>> import numpy as np
 * >>> a = np.array([64,  64, -64], dtype=np.int8)
 * >>> b = np.array([64, -64,  64], dtype=np.int8)
 * >>> mul_q8(a, b).tolist()
 * [32, -32, -32]
 * @endcode
 */
void mul_q8(const int8_t *a, size_t a_len, const int8_t *b, size_t b_len, int8_t *out);


/**
 * @brief Inner product of two Q8 arrays, returning the raw Q14 sum.
 * Accumulates products as int32_t — no scaling or saturation is applied.
 * The result lives in Q14 space (each product is Q7*Q7 = Q14); the caller
 * shifts right 7 to recover a Q7 scalar if needed.
 *
 * @param a      First input array (int8_t).
 * @param a_len  Number of elements in a.
 * @param b      Second input array (int8_t), same length as a.
 * @param b_len  Number of elements in b.
 * @return Raw Q14 accumulation (int32_t).
 *
 * @code
 * >>> from doppler.arith import dot_q8
 * >>> import numpy as np
 * >>> a = np.array([10, 20, 30], dtype=np.int8)
 * >>> b = np.array([1, 2, 3], dtype=np.int8)
 * >>> dot_q8(a, b)
 * 140
 * @endcode
 */
int32_t dot_q8(const int8_t *a, size_t a_len, const int8_t *b, size_t b_len);


/**
 * @brief Elementwise arithmetic left shift of a Q8 array with saturation.
 * Computes out[i] = sat8(a[i] << n). Shifts that would exceed the int8_t
 * range are clamped, preventing silent wraparound into wrong-sign results.
 *
 * @param a      Input array (int8_t).
 * @param a_len  Number of elements in a.
 * @param out    Output array (int8_t), same length as a.
 * @param n      Shift count (non-negative integer).
 *
 * @code
 * >>> from doppler.arith import shl_q8
 * >>> import numpy as np
 * >>> a = np.array([10, 50, 64], dtype=np.int8)
 * >>> shl_q8(a, 1).tolist()
 * [20, 100, 127]
 * @endcode
 */
void shl_q8(const int8_t *a, size_t a_len, int8_t *out, int n);


/**
 * @brief Elementwise arithmetic right shift of a Q8 array with
 * round-half-up. Adds 1 << (n-1) as a rounding bias before shifting,
 * matching the rounding convention of shr_q15 at 8-bit precision.
 *
 * @param a      Input array (int8_t).
 * @param a_len  Number of elements in a.
 * @param out    Output array (int8_t), same length as a.
 * @param n      Shift count (non-negative integer).
 *
 * @code
 * >>> from doppler.arith import shr_q8
 * >>> import numpy as np
 * >>> a = np.array([10, 11, 12, -10], dtype=np.int8)
 * >>> shr_q8(a, 2).tolist()
 * [3, 3, 3, -2]
 * @endcode
 */
void shr_q8(const int8_t *a, size_t a_len, int8_t *out, int n);


/**
 * @brief Elementwise logical left shift of an int64_t array.
 * No saturation is applied — the caller is responsible for ensuring that
 * no element will overflow 64 bits. Shifts >= 63 produce zero. Designed
 * to scale int64_t accumulators (e.g. after a chain of multiply-accumulate
 * operations) before final truncation.
 *
 * @param a      Input array (int64_t).
 * @param a_len  Number of elements in a.
 * @param out    Output array (int64_t), same length as a.
 * @param n      Shift count (non-negative integer; >= 63 yields 0).
 *
 * @code
 * >>> from doppler.arith import shl_i64
 * >>> import numpy as np
 * >>> a = np.array([100, 200, -200], dtype=np.int64)
 * >>> shl_i64(a, 3).tolist()
 * [800, 1600, -1600]
 * @endcode
 */
void shl_i64(const int64_t *a, size_t a_len, int64_t *out, int n);


/**
 * @brief Elementwise arithmetic right shift of an int64_t array with
 * round-half-up. Adds 1 << (n-1) as a bias before shifting, so values
 * at the half-LSB boundary round up. Primarily used to normalise the
 * raw Q30 output of dot_q15 back to Q15 by shifting right 15 bits.
 *
 * @param a      Input array (int64_t).
 * @param a_len  Number of elements in a.
 * @param out    Output array (int64_t), same length as a.
 * @param n      Shift count (non-negative integer; >= 63 is clamped to 63).
 *
 * @code
 * >>> from doppler.arith import dot_q15, shr_i64
 * >>> import numpy as np
 * >>> raw = dot_q15(
 * ...     np.array([16384, 16384], dtype=np.int16),
 * ...     np.array([16384, 16384], dtype=np.int16),
 * ... )
 * >>> shr_i64(np.array([raw], dtype=np.int64), 15).tolist()
 * [16384]
 * @endcode
 */
void shr_i64(const int64_t *a, size_t a_len, int64_t *out, int n);


#ifdef __cplusplus
}
#endif

#endif /* ARITH_CORE_H */
