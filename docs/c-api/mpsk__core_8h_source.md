

# File mpsk\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**mpsk**](dir_c69176d330fb67baeda366bef5232651.md) **>** [**mpsk\_core.h**](mpsk__core_8h.md)

[Go to the documentation of this file](mpsk__core_8h.md)


```C++

#ifndef DP_MPSK_CORE_H
#define DP_MPSK_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/dp_complex.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifndef MPSK_PI
#define MPSK_PI 3.14159265358979323846
#endif

JM_FORCEINLINE int
mpsk_bps (int m)
{
  return (m == 2) ? 1 : (m == 4) ? 2 : (m == 8) ? 3 : 0;
}

JM_FORCEINLINE double
mpsk_phi0 (int m)
{
  return (m == 4) ? (MPSK_PI / 4.0) : 0.0;
}

JM_FORCEINLINE unsigned
mpsk_gray_encode (unsigned k)
{
  return k ^ (k >> 1);
}

JM_FORCEINLINE unsigned
mpsk_gray_decode (unsigned g)
{
  unsigned k = g;
  while (g >>= 1)
    k ^= g;
  return k;
}

JM_FORCEINLINE float _Complex
mpsk_constellation (unsigned g, int m)
{
  unsigned k    = mpsk_gray_decode (g & (unsigned)(m - 1));
  double   theta = 2.0 * MPSK_PI * (double)k / (double)m + mpsk_phi0 (m);
  return (float)cos (theta) + (float)sin (theta) * I;
}

JM_FORCEINLINE unsigned
mpsk_slice (float _Complex y, int m, float _Complex *ahat)
{
  double phi0 = mpsk_phi0 (m);
  double th   = atan2 ((double)cimagf (y), (double)crealf (y)) - phi0;
  long   k    = lround (th * (double)m / (2.0 * MPSK_PI));
  unsigned ki = (unsigned)(k & (long)(m - 1)); /* mod M (M power of two) */
  double   ta = 2.0 * MPSK_PI * (double)ki / (double)m + phi0;
  *ahat       = (float)cos (ta) + (float)sin (ta) * I;
  return mpsk_gray_encode (ki);
}

JM_FORCEINLINE uint8_t
mpsk_bpsk_bit (double re)
{
  return (re < 0.0) ? 1u : 0u;
}

void dp_mpsk_map(const uint8_t *sym, size_t sym_len, float _Complex *out, int m);

void dp_mpsk_demap(const float _Complex *x, size_t x_len, uint8_t *out, int m);

void dp_mpsk_diff_map(const uint8_t *sym, size_t sym_len, float _Complex *out,
                   int m);

void dp_mpsk_diff_demap(const float _Complex *x, size_t x_len, uint8_t *out, int m);

void dp_mpsk_soft_demap(const float _Complex *x, size_t x_len, float *llr,
                     size_t llr_len, int m, float n0);

int dp_mpsk_bits_per_symbol(int m);

/* ── QAM: square constellations beside the PSK ones ──────────────────────
 *
 * Square M-QAM for M = 4, 16, 64, 256, 1024: the same Gray-labelled map and
 * hard decision as the PSK above, for the constellations the multicarrier
 * standards carry on each subcarrier. A symbol's `log2(M)` bits pack
 * LSB-first into one **16-bit** label (1024-QAM is 10 bits, so the byte
 * `mpsk` uses is too small), in the order the bits are transmitted.
 *
 * ## The mapping, and what it is derived from
 *
 * Each axis carries `b = log2(M)/2` bits. Label bits `0 .. b-1` are the
 * in-phase axis and bits `b .. 2b-1` the quadrature axis, in transmission
 * order. On an axis the FIRST bit is the sign — the same polarity as
 * mpsk_bpsk_bit(): 0 is the positive half-plane, 1 the negative — and the
 * `b` bits, read as a number with the first bit most significant, are the
 * Gray code of the level's index counted from the largest positive level
 * down. So adjacent levels differ in exactly one bit, on both axes at once
 * (a slip to a neighbor is one bit error), and QAM-4 is QPSK point for point:
 * `qam_constellation (g, 4)` equals `mpsk_constellation (g, 4)`. 16-QAM per
 * axis reads `00 -> +3, 01 -> +1, 11 -> -1, 10 -> -3`.
 *
 * Levels are the odd integers `-(L-1) .. +(L-1)`, `L = 2^b`, scaled to
 * **unit average energy**: `1/sqrt(2 (M-1) / 3)`, which is `1/sqrt(2)` at
 * M = 4 and `1/sqrt(10)` at M = 16. That figure is derived, not recalled:
 * the mean of the squared odd integers on one axis is `(L^2 - 1) / 3`.
 *
 * **No standard is claimed.** Every standard that carries QAM prescribes its
 * own table from bit groups to levels, and the bit order and the sign of
 * each such table are properties of its text. This mapping is the library's
 * one convention; a PHY that needs another pins that table from the
 * standard, by relabeling at its own boundary or by its own table, and says
 * so. Nothing here conforms to a standard until its numbers are checked.
 */

#define QAM_BPS_MAX 10

JM_FORCEINLINE int
qam_bps (int m)
{
  return (m == 4)     ? 2
         : (m == 16)  ? 4
         : (m == 64)  ? 6
         : (m == 256) ? 8
         : (m == 1024) ? 10
                       : 0;
}

JM_FORCEINLINE double
qam_scale (int m)
{
  return 1.0 / sqrt (2.0 * (double)(m - 1) / 3.0);
}

JM_FORCEINLINE unsigned
qam_axis_gray (unsigned r, int b)
{
  unsigned g = 0;
  for (int t = 0; t < b; t++)
    g |= ((r >> t) & 1u) << (b - 1 - t);
  return g;
}

JM_FORCEINLINE float _Complex
qam_constellation (unsigned g, int m)
{
  const int nb = qam_bps (m);
  if (nb == 0)
    return 0.0f;
  const int      b    = nb / 2;
  const unsigned mask = (1u << b) - 1u;
  /* index from the largest positive level down: j = gray_decode (g) */
  const int    L  = 1 << b;
  const int    ji = (int)mpsk_gray_decode (qam_axis_gray (g & mask, b));
  const int    jq = (int)mpsk_gray_decode (qam_axis_gray ((g >> b) & mask, b));
  const double s  = qam_scale (m);
  return (float)(s * (double)(L - 1 - 2 * ji))
         + (float)(s * (double)(L - 1 - 2 * jq)) * I;
}

JM_FORCEINLINE unsigned
qam_slice (float _Complex y, int m, float _Complex *ahat)
{
  const int nb = qam_bps (m);
  if (nb == 0)
    {
      *ahat = 0.0f;
      return 0u;
    }
  const int    b  = nb / 2;
  const int    L  = 1 << b;
  const double s  = qam_scale (m);
  double       ax[2] = { (double)crealf (y), (double)cimagf (y) };
  unsigned     lab[2];
  double       pt[2];
  for (int a = 0; a < 2; a++)
    {
      /* index j from the largest positive level down, nearest, clamped */
      long j = lround (((double)(L - 1) - ax[a] / s) / 2.0);
      if (j < 0)
        j = 0;
      if (j > L - 1)
        j = L - 1;
      lab[a] = qam_axis_gray (mpsk_gray_encode ((unsigned)j), b);
      pt[a]  = s * (double)(L - 1 - 2 * (int)j);
    }
  *ahat = (float)pt[0] + (float)pt[1] * I;
  return lab[0] | (lab[1] << b);
}

void dp_qam_map(const uint16_t *sym, size_t sym_len, float _Complex *out, int m);

void dp_qam_demap(const float _Complex *x, size_t x_len, uint16_t *out, int m);

int dp_qam_bits_per_symbol(int m);


#ifdef __cplusplus
}
#endif

#endif /* MPSK_CORE_H */
```


