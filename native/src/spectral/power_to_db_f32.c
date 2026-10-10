/*
 * power_to_db_f32.c — spectral module-level function: linear power to dB,
 * the library's one fast dB conversion (#2094, #2074).
 *
 * The contract is the header's. How it is met:
 *
 *   - log2(x) = e + log2(m), with e and m read from the float's bits. m is
 *     reduced to [sqrt(1/2), sqrt(2)) so that t = m - 1 is centred on 0,
 *     and log2(1 + t) is t * q(t), q a cubic fitted (iteratively reweighted
 *     least squares, close to minimax) over that interval. Writing it as
 *     t * q(t) makes log2(1) exactly 0, so every power of two converts
 *     exactly. The fit's own error is 1.0e-4 in log2, 3.1e-4 dB; degree 2
 *     would be 2.6e-3 dB, inside the 0.01 dB contract but by a factor of
 *     four, not thirty.
 *   - The last step, (e + log2 m) * 10 log10(2), is taken in double and
 *     rounded once. In float, a split constant (hi + lo) would make k * 10
 *     log10(2) exact for every power of two, but -ffast-math is free to
 *     re-associate k*hi + k*lo back into k*(hi + lo), and did: 14 powers of
 *     two missed by an ulp. The double product survives it, and still
 *     vectorizes (cvtps2pd, mulpd).
 *   - One select makes anything below 1e-20 (0, a subnormal, a negative
 *     value) read exactly -200, and clamps a value just above the floor
 *     that the polynomial would read a hair below it.
 *
 * No branch, so the loop vectorizes in every shipped build.
 */
#include "doppler/spectral/spectral_core.h"

#include <stdint.h>
#include <string.h>

/* log2(1 + t) ~ t * (C1 + t * (C2 + t * (C3 + t * C4))), t in
   [sqrt(1/2) - 1, sqrt(2) - 1) */
#define P2DB_C1 1.4417624504100373f
#define P2DB_C2 -0.7249018354269552f
#define P2DB_C3 0.5174742774412384f
#define P2DB_C4 -0.3296078261832165f
#define P2DB_SQRT2 1.41421356237309505f
#define P2DB_DB_PER_OCTAVE 3.0102999566398120 /* 10 log10(2), in double */
#define P2DB_FLOOR_LIN 1e-20f                 /* PSD's floor ... */
#define P2DB_FLOOR_DB -200.0f                 /* ... in dB */

void
dp_power_to_db_f32 (const float *lin, size_t lin_len, float *out)
{
  for (size_t i = 0; i < lin_len; i++)
    {
      const float x = lin[i];
      uint32_t    b;
      memcpy (&b, &x, sizeof b);
      int32_t  e  = (int32_t)(b >> 23) - 127;
      uint32_t mb = (b & 0x007FFFFFu) | 0x3F800000u; /* m in [1, 2) */
      float    m;
      memcpy (&m, &mb, sizeof m);
      const int above = m > P2DB_SQRT2;
      m               = above ? m * 0.5f : m;
      e += above;
      const float t = m - 1.0f;
      const float l
          = t * (P2DB_C1 + t * (P2DB_C2 + t * (P2DB_C3 + t * P2DB_C4)));
      const float d = (float)(((double)e + (double)l) * P2DB_DB_PER_OCTAVE);
      out[i] = (x < P2DB_FLOOR_LIN || d < P2DB_FLOOR_DB) ? P2DB_FLOOR_DB : d;
    }
}
