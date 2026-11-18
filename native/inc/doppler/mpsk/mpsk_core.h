/**
 * @file mpsk_core.h
 * @brief M-PSK constellation: Gray-coded map / demap for BPSK, QPSK, 8PSK.
 *
 * The receive-side decision primitive (and its transmit inverse) for M-ary
 * phase-shift keying. A symbol carries `log2(M)` bits packed LSB-first into one
 * byte (0..M-1); the byte IS the **Gray-coded** label, so a slip to an adjacent
 * constellation point flips exactly one bit. Unit amplitude; constellation:
 *
 *   - BPSK (M=2): {+1, -1}            (phi0 = 0)
 *   - QPSK (M=4): (+-1 +- j)/sqrt(2)  (phi0 = pi/4, axis-separable I/Q)
 *   - 8PSK (M=8): exp(j*k*pi/4)       (phi0 = 0)
 *
 * The inline helpers (mpsk_constellation, mpsk_slice) are the C composition API
 * a carrier loop / receiver inlines per symbol; the module free functions
 * (mpsk_map/mpsk_demap and the differential variants) are the array Python face.
 * Memoryless functions are element-wise (one byte <-> one cf32); the differential
 * variants carry phase state across the array (info on phase *differences*,
 * which removes the M-fold carrier ambiguity).
 *
 * @code
 * // C: round-trip one QPSK symbol through the inline slicer
 * float _Complex ahat;
 * unsigned g = mpsk_slice((1.0f + 1.0f*I) * 0.70710678f, 4, &ahat); // -> 0
 * @endcode
 */
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
/** @brief pi, in double precision, for the M-PSK phase arithmetic. */
#define MPSK_PI 3.14159265358979323846
#endif

/**
 * @brief Bits per M-PSK symbol = log2(M).
 * @param m  Modulation order M; 2, 4 or 8.
 * @return 1, 2 or 3 for M = 2, 4, 8; 0 for any other @p m (unsupported).
 */
JM_FORCEINLINE int
mpsk_bps (int m)
{
  return (m == 2) ? 1 : (m == 4) ? 2 : (m == 8) ? 3 : 0;
}

/**
 * @brief Constellation phase offset of the M-PSK alphabet.
 * @param m  Modulation order M.
 * @return pi/4 rad for M = 4 (QPSK points on the diagonals); 0 rad for
 *         every other @p m.
 */
JM_FORCEINLINE double
mpsk_phi0 (int m)
{
  return (m == 4) ? (MPSK_PI / 4.0) : 0.0;
}

/**
 * @brief Binary index -> Gray code (k ^ k>>1).
 * @param k  Binary constellation index.
 * @return Gray code of @p k; adjacent indices differ in one bit.
 */
JM_FORCEINLINE unsigned
mpsk_gray_encode (unsigned k)
{
  return k ^ (k >> 1);
}

/**
 * @brief Gray code -> binary index (inverse of mpsk_gray_encode).
 * @param g  Gray-coded label.
 * @return Binary index k with mpsk_gray_encode(k) == @p g.
 */
JM_FORCEINLINE unsigned
mpsk_gray_decode (unsigned g)
{
  unsigned k = g;
  while (g >>= 1)
    k ^= g;
  return k;
}

/**
 * @brief Constellation point for Gray label @p g (M-PSK), unit amplitude.
 *
 * Maps the Gray-coded label byte (0..M-1) to its complex point
 * `exp(j*(2*pi*k/M + phi0))`, where `k = gray_decode(g)` is the constellation
 * index. Inverse of mpsk_slice()'s returned label.
 *
 * @param g  Gray label, masked to the low log2(M) bits.
 * @param m  M in {2,4,8}.
 * @return   Unit-amplitude constellation point.
 *
 * @note C only, with mpsk_slice(): both bodies use the C99 complex library,
 *   which C++ does not have (dp_complex.h, "From C++"). A C++ caller uses
 *   the array functions, mpsk_map() and mpsk_demap().
 */
#ifndef __cplusplus
JM_FORCEINLINE float _Complex
mpsk_constellation (unsigned g, int m)
{
  unsigned k    = mpsk_gray_decode (g & (unsigned)(m - 1));
  double   theta = 2.0 * MPSK_PI * (double)k / (double)m + mpsk_phi0 (m);
  return (float)cos (theta) + (float)sin (theta) * I;
}

/**
 * @brief Hard-decide @p y to the nearest M-PSK point; return its Gray label.
 *
 * Picks the constellation index nearest in phase, writes that unit-amplitude
 * point to @p ahat (the decision, for a decision-directed carrier error
 * `Im(y * conj(ahat))`), and returns the Gray-coded label byte. Inverse of
 * mpsk_constellation(). One atan2 per call (symbol-rate, not the sample loop).
 *
 * @param y     Received symbol (any amplitude; only the phase is used).
 * @param m     M in {2,4,8}.
 * @param ahat  Out: the nearest unit-amplitude constellation point.
 * @return      Gray-coded label (0..M-1).
 */
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
#endif /* !__cplusplus */

/**
 * @brief BPSK hard decision on a real value: negative is bit 1.
 *
 * The library's ONE bit <-> symbol polarity, stated where every caller can
 * inline it. A bit @c b is sent as `1 - 2*b` -- bit 0 is +1, bit 1 is -1,
 * which is mpsk_constellation() at m = 2 -- so the decision that inverts it
 * maps a NEGATIVE value to 1 and zero or positive to 0. Exactly zero decides
 * to 0, so the rule is total and a round trip through dp_bin_to_nrz() is
 * exact.
 *
 * Every object that turns a real correlation into a bit calls this rather
 * than writing its own comparison: dp_nrz_to_bin(), and the despreaders'
 * bits(). Two copies of a one-line comparison are how two objects came to
 * emit opposite bits for the same signal (doppler#2654), and
 * test_bit_polarity.py runs one known-phase BPSK stream through every bit
 * emitter and requires they agree.
 *
 * @param re  The real part of a carrier-wiped prompt, an NRZ symbol, or any
 *            sum of them.
 * @return    1 if @p re < 0, else 0.
 */
JM_FORCEINLINE uint8_t
mpsk_bpsk_bit (double re)
{
  return (re < 0.0) ? 1u : 0u;
}

/**
 * @brief Map Gray-coded M-PSK labels to unit-amplitude constellation points.
 *
 * Element-wise inverse of dp_mpsk_demap(): each input byte is one symbol's
 * log2(M) Gray-coded bits (0..M-1), each output is its cf32 point. Memoryless
 * (absolute phase). @p out must hold @p sym_len points.
 *
 * @param sym      Gray label bytes (0..M-1), one per symbol.
 * @param sym_len  Number of symbols.
 * @param out      Out: @p sym_len constellation points.
 * @param m        M in {2,4,8}.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.mpsk import mpsk_map, mpsk_demap
 * >>> sym = np.array([0, 1, 2, 3], dtype=np.uint8)   # QPSK labels
 * >>> pts = mpsk_map(sym, 4)
 * >>> np.round(np.abs(pts), 5)
 * array([1., 1., 1., 1.], dtype=float32)
 * >>> np.array_equal(mpsk_demap(pts, 4), sym)
 * True
 *
 * @endcode
 */
void dp_mpsk_map(const uint8_t *sym, size_t sym_len, float _Complex *out, int m);

/**
 * @brief Hard-decide M-PSK symbols to their Gray-coded label bytes.
 *
 * Element-wise inverse of dp_mpsk_map(): each cf32 symbol is sliced to the nearest
 * constellation point and its Gray label (0..M-1) is written out. A slip to an
 * adjacent point flips exactly one bit (Gray). @p out must hold @p x_len bytes.
 *
 * @param x      Received symbols (any amplitude; phase only).
 * @param x_len  Number of symbols.
 * @param out    Out: @p x_len Gray label bytes.
 * @param m      M in {2,4,8}.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.mpsk import mpsk_demap
 * >>> x = np.array([1+0j, 1j, -1+0j, -1j], dtype=np.complex64)  # 8PSK
 * >>> mpsk_demap(x, 8).tolist()   # Gray labels of indices 0, 2, 4, 6
 * [0, 3, 6, 5]
 *
 * @endcode
 */
void dp_mpsk_demap(const float _Complex *x, size_t x_len, uint8_t *out, int m);

/**
 * @brief Differential M-PSK map: the label selects a phase INCREMENT.
 *
 * Information rides on phase *differences*: the running constellation index
 * accumulates `gray_decode(label)` each symbol (starting from an implicit
 * zero-phase reference), so an unknown constant carrier phase cancels at the
 * receiver (mpsk_diff_demap) — resolving the M-fold ambiguity. Sequential
 * over the array.
 *
 * The cost is up to **2x the symbol-error rate** of coherent map(). That
 * factor is a high-SNR asymptote, not a constant: measured, BPSK and QPSK
 * reach it by ~8 dB Es/N0 while 8PSK pays only 1.44x at 4 dB and 2.03x by
 * 14 dB. A caller sizing a link at low Es/N0 is charged less than the round
 * number suggests (native/validation/mpsk_diff_penalty.c).
 *
 * @param sym      Gray label bytes (0..M-1), one per symbol.
 * @param sym_len  Number of symbols.
 * @param out      Out: @p sym_len constellation points.
 * @param m        M in {2,4,8}.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.mpsk import mpsk_diff_map, mpsk_diff_demap
 * >>> sym = np.array([1, 0, 3, 2, 1], dtype=np.uint8)
 * >>> pts = mpsk_diff_map(sym, 4)
 * >>> np.array_equal(mpsk_diff_demap(pts, 4), sym)   # exact round-trip
 * True
 * >>> rot = (pts * np.exp(1j * np.pi / 2)).astype(np.complex64)  # slip
 * >>> np.array_equal(mpsk_diff_demap(rot, 4)[1:], sym[1:])  # invariant
 * True
 *
 * @endcode
 */
void dp_mpsk_diff_map(const uint8_t *sym, size_t sym_len, float _Complex *out,
                   int m);

/**
 * @brief Differential M-PSK demap: decide from the phase DIFFERENCE.
 *
 * Inverse of dp_mpsk_diff_map(): the Gray label of each symbol is decided from the
 * phase difference between consecutive sliced indices (the first references an
 * implicit zero-phase start). Invariant to an unknown constant carrier phase.
 *
 * @param x      Received symbols (any amplitude; phase only).
 * @param x_len  Number of symbols.
 * @param out    Out: @p x_len Gray label bytes.
 * @param m      M in {2,4,8}.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.mpsk import mpsk_diff_demap, mpsk_diff_map
 * >>> sym = np.array([2, 2, 1, 0], dtype=np.uint8)
 * >>> np.array_equal(mpsk_diff_demap(mpsk_diff_map(sym, 8), 8), sym)
 * True
 *
 * @endcode
 */
void dp_mpsk_diff_demap(const float _Complex *x, size_t x_len, uint8_t *out, int m);

/**
 * @brief Soft-demap M-PSK symbols to per-bit log-likelihood ratios.
 *
 * The soft counterpart of dp_mpsk_demap(): instead of one label byte per symbol
 * it writes `log2(M)` LLRs, one per bit, which is what a soft-input decoder
 * (a Viterbi, for the CCSDS inner code) needs. A hard decision throws away
 * roughly 2 dB of the coding gain such a decoder exists to deliver.
 *
 * The convention, which every consumer has to agree with:
 *
 *     L_i = log( P(bit i = 0 | y) / P(bit i = 1 | y) )
 *
 * so **positive means bit 0** and the hard decision is `L < 0`. That is not a
 * separate rule: `dp_mpsk_demap()` is what this reproduces, and the sign
 * agreeing with it at every M and every SNR is asserted in test_mpsk_core.c
 * rather than assumed. The repository has ONE decision rule; this is a second
 * view of it, not a second copy.
 *
 * Bits are LSB-first within a symbol, matching how the Gray label packs them,
 * and symbols run in order:
 * `llr[i * log2(M) + b]` is bit @c b of symbol @c i.
 *
 * Computed by the max-log rule over the constellation
 * `L_i = (min_{b_i=1} |y-a|^2 - min_{b_i=0} |y-a|^2) / n0`. For BPSK and QPSK
 * this is EXACT — QPSK's `phi0 = pi/4` grid is axis-separable, so its two
 * bits are independent BPSK decisions and each subset holds one point. Only
 * 8PSK is an approximation; what that costs in dB is not measured yet and is
 * therefore not claimed here (docs/design/mpsk.md §9.7).
 *
 * @p n0 is the noise power `E[|n|^2]` for unit-amplitude symbols, and it
 * scales the output exactly: `L(n0) = L(1) / n0`. A **Viterbi is invariant to
 * it**, since scaling every branch metric by a positive constant cannot move
 * the maximum-likelihood path — so a caller with no SNR estimate may pass 1.0
 * and get correctly ordered, unscaled soft values.
 *
 * @param x        Received symbols (amplitude matters here — unlike the hard
 *                 path, which uses phase only).
 * @param x_len    Number of symbols.
 * @param llr      Out: @p x_len * log2(M) LLRs.
 * @param llr_len  Capacity of @p llr. Nothing is written if it is short, or
 *                 if @p m is unsupported.
 * @param m        M in {2,4,8}.
 * @param n0       Noise power `E[|n|^2]`; must be positive.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.mpsk import mpsk_soft_demap, mpsk_demap
 * >>> x = np.array([0.9+0.1j, -0.8-0.2j], dtype=np.complex64)   # BPSK
 * >>> llr = np.empty(2, dtype=np.float32)
 * >>> mpsk_soft_demap(x, llr, 2, 1.0)
 * >>> np.round(llr, 3)                       # 4*Re(y)/n0
 * array([ 3.6, -3.2], dtype=float32)
 * >>> np.array_equal((llr < 0).astype(np.uint8), mpsk_demap(x, 2))
 * True
 *
 * @endcode
 */
void dp_mpsk_soft_demap(const float _Complex *x, size_t x_len, float *llr,
                     size_t llr_len, int m, float n0);

/**
 * @brief Bits per M-PSK symbol = log2(M).
 *
 * @param m  M in {2,4,8}.
 * @return   1, 2, or 3 (0 for an unsupported M).
 *
 * @code
 * >>> from doppler.mpsk import mpsk_bits_per_symbol
 * >>> [mpsk_bits_per_symbol(m) for m in (2, 4, 8)]
 * [1, 2, 3]
 *
 * @endcode
 */
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

/** @brief Largest QAM bits per symbol (1024-QAM). */
#define QAM_BPS_MAX 10

/**
 * @brief Bits per QAM symbol = log2(M).
 * @param m  Constellation size: 4, 16, 64, 256 or 1024.
 * @return 2, 4, 6, 8 or 10; 0 for any other @p m (unsupported).
 */
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

/**
 * @brief The factor that gives a square M-QAM unit average energy.
 * @param m  Constellation size.
 * @return `1 / sqrt (2 (M - 1) / 3)`.
 */
JM_FORCEINLINE double
qam_scale (int m)
{
  return 1.0 / sqrt (2.0 * (double)(m - 1) / 3.0);
}

/**
 * @brief The first @p b bits of @p r as a number, first bit most
 *        significant (the reverse of how the label packs them).
 *
 * The axis's bits sit LSB-first in the label, in transmission order; the
 * Gray code of a level index reads with the first transmitted bit as its
 * top bit. This is the one place that reversal is written, used by both
 * qam_constellation() and qam_slice().
 *
 * @param r  Label bits for one axis (only the low @p b are read).
 * @param b  Bits per axis, log2(M) / 2.
 * @return   The @p b bits reversed, as an integer in `[0, 2^b)`.
 */
JM_FORCEINLINE unsigned
qam_axis_gray (unsigned r, int b)
{
  unsigned g = 0;
  for (int t = 0; t < b; t++)
    g |= ((r >> t) & 1u) << (b - 1 - t);
  return g;
}

/**
 * @brief Constellation point for label @p g (square QAM), unit average
 *        energy.
 *
 * @param g  Label, masked to the low log2(M) bits.
 * @param m  M in {4, 16, 64, 256, 1024}.
 * @return   The point; 0 for an unsupported @p m.
 *
 * @note C only, with qam_slice(): both bodies use the C99 complex library,
 *   which C++ does not have (dp_complex.h, "From C++"). A C++ caller uses
 *   the array functions, dp_qam_map() and dp_qam_demap().
 */
#ifndef __cplusplus
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

/**
 * @brief Hard-decide @p y to the nearest QAM point; return its label.
 *
 * The axes decide independently: each is quantized to the nearest level and
 * clamped to the outermost, which for a square constellation IS the nearest
 * point. Writes that point to @p ahat. Inverse of qam_constellation().
 *
 * @param y     Received symbol; amplitude matters (unlike M-PSK).
 * @param m     M in {4, 16, 64, 256, 1024}.
 * @param ahat  Out: the nearest constellation point; 0 for an unsupported m.
 * @return      The label (0 .. M-1); 0 for an unsupported @p m.
 */
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
#endif /* !__cplusplus */

/**
 * @brief Map QAM labels to unit-average-energy constellation points.
 *
 * Element-wise inverse of dp_qam_demap(): each input is one symbol's
 * `log2(M)` bits packed LSB-first in transmission order, each output its cf32
 * point. @p out must hold @p sym_len points. For an unsupported @p m every
 * output is 0, which is not a constellation point of any supported M, so
 * check dp_qam_bits_per_symbol() first.
 *
 * @param sym      Labels (0 .. M-1), one per symbol, 16-bit.
 * @param sym_len  Number of symbols.
 * @param out      Out: @p sym_len constellation points.
 * @param m        M in {4, 16, 64, 256, 1024}.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.mpsk import qam_map, qam_demap
 * >>> sym = np.array([0, 1, 3, 2], dtype=np.uint16)   # 16-QAM, I axis
 * >>> np.round(qam_map(sym, 16).real * np.sqrt(10), 3)
 * array([ 3., -3., -1.,  1.])
 * >>> np.array_equal(qam_demap(qam_map(sym, 16), 16), sym)
 * True
 *
 * @endcode
 */
void dp_qam_map(const uint16_t *sym, size_t sym_len, float _Complex *out, int m);

/**
 * @brief Hard-decide QAM symbols to their labels.
 *
 * Element-wise inverse of dp_qam_map(). Each axis is decided on its own to
 * the nearest level, so a received point outside the constellation lands on
 * the nearest edge point. For an unsupported @p m every label is 0.
 *
 * @param x      Received symbols; amplitude matters.
 * @param x_len  Number of symbols.
 * @param out    Out: @p x_len labels.
 * @param m      M in {4, 16, 64, 256, 1024}.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.mpsk import qam_demap
 * >>> v = np.array([3.2, -0.9, 0.2, -2.7]) / np.sqrt(10)
 * >>> x = (v + 1j * v[::-1]).astype(np.complex64)
 * >>> qam_demap(x, 16).tolist()
 * [4, 11, 14, 1]
 *
 * @endcode
 */
void dp_qam_demap(const float _Complex *x, size_t x_len, uint16_t *out, int m);

/**
 * @brief Bits per QAM symbol = log2(M).
 *
 * @param m  M in {4, 16, 64, 256, 1024}.
 * @return   2, 4, 6, 8 or 10 (0 for an unsupported M).
 *
 * @code
 * >>> from doppler.mpsk import qam_bits_per_symbol
 * >>> [qam_bits_per_symbol(m) for m in (4, 16, 64, 256, 1024)]
 * [2, 4, 6, 8, 10]
 *
 * @endcode
 */
int dp_qam_bits_per_symbol(int m);


#ifdef __cplusplus
}
#endif

#endif /* MPSK_CORE_H */
