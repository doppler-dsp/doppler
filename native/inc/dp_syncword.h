/**
 * @file dp_syncword.h
 * @brief Finding a known bit pattern in an unpacked bit stream — the sync
 * word search, and the arithmetic for choosing its threshold.
 *
 * A frame synchroniser correlates a marker it knows against the bits it is
 * handed, in both polarities, and reports the first offset close enough to
 * accept. That is one kernel, and every framing that has a sync word wants
 * it: CCSDS calls its 32-bit marker an ASM and `ccsds_tm_asm_find` is this
 * function configured with `0x1ACFFC1D`, exactly as `CCSDS_TM_CONV`
 * configures `conv_code_t`. The standard picks a pattern; the search is not
 * the standard's.
 *
 * Header-only (like `dp_crc16.h`) so no component grows a link-line
 * dependency for a kernel this size — a receiver correlating a marker
 * should not link a Reed-Solomon encoder to do it.
 *
 * Bit convention: **unpacked** bits, one per byte in the LSB, which is what
 * `wfm_frame_bits`, `dp_crc16_ccitt`, `ccsds_tm_randomise` and the spreader
 * already pass around.
 *
 * ## Choosing @p max_errors — it is not a property of the marker
 *
 * The threshold is the whole of the trade, and the number a caller needs is
 * a function of **how much stream they search**, not of how long the marker
 * is. A 32-bit marker invites "half of 32 is 16, so 8 sounds safe", and 8
 * finds the marker at its true offset only 58 % of the time on a stream with
 * no channel errors at all — because each preceding offset is an independent
 * chance to false-hit first, and the search reports the FIRST acceptable
 * offset rather than the best one.
 *
 * So the arithmetic ships beside the search. Per offset, a random window
 * lands within @p t of an @p n -bit marker in one polarity or the other with
 *
 *   P_fa(n, t) = 2 * sum_{i <= t} C(n, i) / 2^n      (`dp_syncword_pfa`)
 *
 * and over @p W offsets tried ahead of the true marker the chance one of
 * them wins the race is `1 - (1 - P_fa)^W`. `dp_syncword_max_errors`
 * inverts that: give it the window and the false-frame rate you will accept
 * and it returns the largest threshold that holds.
 *
 * Measured for the 32-bit CCSDS marker (doppler#897, and
 * `src/doppler/tests/validation/ccsds_tm/results.md` §2.2–2.3): the measured
 * false-alarm rate tracks the closed form above to within 20 % at every
 * threshold where the count supports a rate, `t <= 1` produced zero false
 * markers in 2000000 random bits, and polarity was never once reported
 * wrong in 155901 detections.
 */
#ifndef DP_SYNCWORD_H
#define DP_SYNCWORD_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Where a marker was found, and in which polarity.
 *
 * @c inverted is not a curiosity. A BPSK carrier recovered by a loop with a
 * 180-degree ambiguity delivers the whole stream complemented, and a marker
 * that no randomiser covers is the only thing in a frame that can say so —
 * it looks the same in every frame and in exactly one polarity.
 */
typedef struct
{
  size_t   offset;   /**< Bit index where the marker starts       */
  int      inverted; /**< The stream is complemented              */
  unsigned errors;   /**< Hamming distance to the marker there    */
} dp_syncword_hit_t;

/**
 * @brief Find the first marker in a run of unpacked bits, either polarity.
 *
 * Correlates @p marker against every bit offset and against its complement,
 * and reports the **first** offset whose Hamming distance is at most
 * @p max_errors.
 *
 * First rather than best, and the difference matters: a best-match search
 * has to see the whole stream before it can answer, which a frame
 * synchroniser reading a live capture cannot do. First-below-threshold is
 * what is implementable in both settings, so it is what this promises — and
 * it is why @p max_errors must be chosen against the search window, as the
 * file comment sets out.
 *
 * @param bits        Unpacked bits, one per byte.
 * @param n_bits      Number of bits.
 * @param marker      The pattern to find, unpacked bits, one per byte.
 * @param n_marker    Length of @p marker in bits.
 * @param max_errors  Largest tolerated Hamming distance, in bits.
 * @param hit         Receives the location; untouched when nothing matched.
 * @return            Non-zero if a marker was found.
 */
static inline int
dp_syncword_find (const uint8_t *bits, size_t n_bits, const uint8_t *marker,
                  size_t n_marker, unsigned max_errors,
                  dp_syncword_hit_t *hit)
{
  if (n_marker == 0u || n_bits < n_marker)
    return 0;

  const size_t last = n_bits - n_marker;
  for (size_t off = 0; off <= last; off++)
    {
      /* Both polarities in one pass: a bit that disagrees with the marker
         agrees with its complement, so the two distances sum to the marker
         length and one comparison yields both. */
      unsigned d = 0;
      for (size_t i = 0; i < n_marker; i++)
        d += (unsigned)((bits[off + i] & 1u) ^ (marker[i] & 1u));

      const unsigned dinv = (unsigned)n_marker - d;
      if (d <= max_errors || dinv <= max_errors)
        {
          const int inv = dinv < d;
          hit->offset   = off;
          hit->inverted = inv;
          hit->errors   = inv ? dinv : d;
          return 1;
        }
    }
  return 0;
}

/**
 * @brief Probability that ONE random offset false-hits an @p n_marker -bit
 * marker at a tolerance of @p max_errors.
 *
 * `P_fa = 2 * sum_{i <= max_errors} C(n, i) / 2^n` — the factor of two
 * because `dp_syncword_find` searches the complement too, and a random
 * window is as likely to land near one polarity as the other. The two
 * events are disjoint while `2 * max_errors < n_marker`; at and above that
 * every window matches in one polarity or the other, and the result is 1.
 *
 * This is the per-offset number. What a synchroniser actually cares about
 * is the whole window it searches — see `dp_syncword_max_errors`, which
 * is this function inverted through `1 - (1 - P_fa)^W`.
 *
 * The terms are summed in log space rather than as binomial coefficients,
 * so a long marker (`C(1030, 515)` overflows a double) is no different from
 * a short one.
 *
 * @param n_marker    Marker length in bits; 0 gives 0.
 * @param max_errors  Tolerance in bits.
 * @return            Probability in &#91;0, 1&#93;.
 */
static inline double
dp_syncword_pfa (size_t n_marker, unsigned max_errors)
{
  if (n_marker == 0u)
    return 0.0;

  const double n     = (double)n_marker;
  const double lgn1  = lgamma (n + 1.0);
  const double lg2   = n * log (2.0);
  const size_t t_max = (size_t)max_errors < n_marker ? (size_t)max_errors
                                                     : n_marker;

  double s = 0.0;
  for (size_t i = 0; i <= t_max; i++)
    s += exp (lgn1 - lgamma ((double)i + 1.0)
              - lgamma (n - (double)i + 1.0) - lg2);

  const double p = 2.0 * s;
  return p > 1.0 ? 1.0 : p;
}

/**
 * @brief The largest tolerance whose false-frame rate over a search window
 * still meets @p pfa.
 *
 * The counterpart of `det_threshold` for this detector, and the answer to
 * the question the signature of `dp_syncword_find` cannot ask: a caller
 * knows how much stream their synchroniser reads before the marker arrives,
 * and that — not the marker length — is what sets the threshold.
 *
 * Every offset ahead of the true marker is an independent chance to win the
 * race, so `P = 1 - (1 - P_fa(n, t))^W`. `P` rises with `t`, so the largest
 * `t` that holds is the most tolerant threshold that keeps the false-frame
 * rate at or under @p pfa.
 *
 * @param n_marker     Marker length in bits.
 * @param window_bits  Offsets tried AHEAD of the marker — the length of
 *                     stream searched, not the length of the frame.
 * @param pfa          Tolerated probability that the window produces a
 *                     false frame.
 * @return             Tolerance in bits, or -1 when even an exact match
 *                     (`t = 0`) exceeds @p pfa over that window.
 */
static inline int
dp_syncword_max_errors (size_t n_marker, size_t window_bits, double pfa)
{
  int best = -1;
  for (size_t t = 0; t <= n_marker; t++)
    {
      const double p = dp_syncword_pfa (n_marker, (unsigned)t);
      /* -expm1(W log1p(-p)) is 1 - (1-p)^W without cancelling to zero at
         the small p that a usable threshold actually produces. */
      const double win = -expm1 ((double)window_bits * log1p (-p));
      if (win > pfa)
        break;
      best = (int)t;
    }
  return best;
}

#ifdef __cplusplus
}
#endif

#endif /* DP_SYNCWORD_H */
