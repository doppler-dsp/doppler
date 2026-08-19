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
 * float complex ahat;
 * unsigned g = mpsk_slice((1.0f + 1.0f*I) * 0.70710678f, 4, &ahat); // -> 0
 * @endcode
 */
#ifndef MPSK_CORE_H
#define MPSK_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include <complex.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifndef MPSK_PI
#define MPSK_PI 3.14159265358979323846
#endif

/** @brief Bits per M-PSK symbol = log2(M); M in {2,4,8} -> {1,2,3}, else 0. */
JM_FORCEINLINE int
mpsk_bps (int m)
{
  return (m == 2) ? 1 : (m == 4) ? 2 : (m == 8) ? 3 : 0;
}

/** @brief Constellation phase offset (radians): pi/4 for QPSK, else 0. */
JM_FORCEINLINE double
mpsk_phi0 (int m)
{
  return (m == 4) ? (MPSK_PI / 4.0) : 0.0;
}

/** @brief Binary index -> Gray code (k ^ k>>1). */
JM_FORCEINLINE unsigned
mpsk_gray_encode (unsigned k)
{
  return k ^ (k >> 1);
}

/** @brief Gray code -> binary index (inverse of mpsk_gray_encode). */
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
 */
JM_FORCEINLINE float complex
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
mpsk_slice (float complex y, int m, float complex *ahat)
{
  double phi0 = mpsk_phi0 (m);
  double th   = atan2 ((double)cimagf (y), (double)crealf (y)) - phi0;
  long   k    = lround (th * (double)m / (2.0 * MPSK_PI));
  unsigned ki = (unsigned)(k & (long)(m - 1)); /* mod M (M power of two) */
  double   ta = 2.0 * MPSK_PI * (double)ki / (double)m + phi0;
  *ahat       = (float)cos (ta) + (float)sin (ta) * I;
  return mpsk_gray_encode (ki);
}

/**
 * @brief Map Gray-coded M-PSK labels to unit-amplitude constellation points.
 *
 * Element-wise inverse of mpsk_demap(): each input byte is one symbol's
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
void mpsk_map(const uint8_t *sym, size_t sym_len, float complex *out, int m);

/**
 * @brief Hard-decide M-PSK symbols to their Gray-coded label bytes.
 *
 * Element-wise inverse of mpsk_map(): each cf32 symbol is sliced to the nearest
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
void mpsk_demap(const float complex *x, size_t x_len, uint8_t *out, int m);

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
void mpsk_diff_map(const uint8_t *sym, size_t sym_len, float complex *out,
                   int m);

/**
 * @brief Differential M-PSK demap: decide from the phase DIFFERENCE.
 *
 * Inverse of mpsk_diff_map(): the Gray label of each symbol is decided from the
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
void mpsk_diff_demap(const float complex *x, size_t x_len, uint8_t *out, int m);

/**
 * @brief Soft-demap M-PSK symbols to per-bit log-likelihood ratios.
 *
 * The soft counterpart of mpsk_demap(): instead of one label byte per symbol
 * it writes `log2(M)` LLRs, one per bit, which is what a soft-input decoder
 * (a Viterbi, for the CCSDS inner code) needs. A hard decision throws away
 * roughly 2 dB of the coding gain such a decoder exists to deliver.
 *
 * The convention, which every consumer has to agree with:
 *
 *     L_i = log( P(bit i = 0 | y) / P(bit i = 1 | y) )
 *
 * so **positive means bit 0** and the hard decision is `L < 0`. That is not a
 * separate rule: `mpsk_demap()` is what this reproduces, and the sign
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
void mpsk_soft_demap(const float complex *x, size_t x_len, float *llr,
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
int mpsk_bits_per_symbol(int m);

#ifdef __cplusplus
}
#endif

#endif /* MPSK_CORE_H */
