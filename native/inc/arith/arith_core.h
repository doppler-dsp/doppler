/**
 * @file arith_core.h
 * @brief Arith module — public C API.
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

void add_q15(const int16_t *a, size_t a_len, const int16_t *b, size_t b_len, int16_t *out);


void sub_q15(const int16_t *a, size_t a_len, const int16_t *b, size_t b_len, int16_t *out);


void mul_q15(const int16_t *a, size_t a_len, const int16_t *b, size_t b_len, int16_t *out);


int64_t dot_q15(const int16_t *a, size_t a_len, const int16_t *b, size_t b_len);


void shl_q15(const int16_t *a, size_t a_len, int16_t *out, int n);


void shr_q15(const int16_t *a, size_t a_len, int16_t *out, int n);


void add_q8(const int8_t *a, size_t a_len, const int8_t *b, size_t b_len, int8_t *out);


void sub_q8(const int8_t *a, size_t a_len, const int8_t *b, size_t b_len, int8_t *out);


void mul_q8(const int8_t *a, size_t a_len, const int8_t *b, size_t b_len, int8_t *out);


int32_t dot_q8(const int8_t *a, size_t a_len, const int8_t *b, size_t b_len);


void shl_q8(const int8_t *a, size_t a_len, int8_t *out, int n);


void shr_q8(const int8_t *a, size_t a_len, int8_t *out, int n);


void shl_i64(const int64_t *a, size_t a_len, int64_t *out, int n);


void shr_i64(const int64_t *a, size_t a_len, int64_t *out, int n);


#ifdef __cplusplus
}
#endif

#endif /* ARITH_CORE_H */
