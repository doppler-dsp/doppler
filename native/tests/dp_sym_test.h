/**
 * @file dp_sym_test.h
 * @brief Truth-free symbol-quality validators for receiver tests.
 *
 * **A bit error rate on its own is not evidence a receiver works.** Every
 * receiver test in this project scores BER by searching over an unknown lag
 * and an unknown phase/polarity ambiguity, and that search is exactly what
 * makes the number untrustworthy at low Es/N0: it can find a lucky alignment
 * on a scattered constellation and report a passing BER (a false pass), and it
 * can miss the true alignment on a perfectly healthy one and report a floor
 * that is an artifact of the search (a false floor). Both have been hit here
 * before.
 *
 * So pair every BER assertion with at least one of these. Neither references
 * the transmitted data, so neither can be fooled by the alignment search:
 *
 *   - dp_test_evm_db_hard() — EVM against the stream's OWN hard decisions,
 *     with the constellation rotation estimated from the data. No lag, no
 *     truth. At the matched-filter output the error vector IS the complex
 *     noise, so a locked BPSK stream reads EVM[dB] ~ -(Es/N0)[dB] and a
 *     scattered one sits near 0 dB. (EVM is an I/Q-plane quantity: there is
 *     no factor of two — that belongs to an I-only measurement, and quoting
 *     it flatters the result by 3 dB.)
 *   - dp_test_m2m4_snr_db() — the blind moment-based estimator, via the
 *     canonical snr_m2m4_db() primitive. Fully independent of the above: a
 *     locked stream recovers ~Es/N0, noise-dominated symbols estimate near 0.
 *
 * An EVM that BEATS the -(Es/N0) bound is the tell that the measurement is
 * wrong, not that the receiver is brilliant.
 *
 * Both score the BACK HALF of the stream, so an acquisition transient (or a
 * single cycle slip during it) cannot drag the steady-state figure — the
 * failure mode that once made a healthy eye read -17 dB.
 */
#ifndef DP_SYM_TEST_H
#define DP_SYM_TEST_H

#include "snr/snr_core.h" /* snr_m2m4_db — the canonical blind estimator */
#include <complex.h>
#include <math.h>
#include <stddef.h>

/* Self-referenced EVM (dB) over the back half, against the stream's own hard
 * decisions after estimating the constellation rotation from the data itself.
 * Returns 0.0 (i.e. "no lock") for a stream too short to judge. */
static inline double
dp_test_evm_db_hard (const float complex *syms, size_t n_syms)
{
  if (n_syms < 20)
    return 0.0;
  size_t lo = n_syms / 2, hi = n_syms, n = hi - lo;
  double c2r = 0.0, c2i = 0.0, p = 0.0;
  for (size_t i = lo; i < hi; i++)
    {
      double re = crealf (syms[i]), im = cimagf (syms[i]);
      c2r += re * re - im * im; /* Re(z^2) */
      c2i += 2.0 * re * im;     /* Im(z^2) */
      p += re * re + im * im;
    }
  double scale = sqrt (p / (double)n);
  if (scale < 1e-20)
    return 0.0;
  double phi = 0.5 * atan2 (c2i, c2r); /* constellation rotation */
  double cr = cos (-phi), sr = sin (-phi);
  double errsq = 0.0;
  for (size_t i = lo; i < hi; i++)
    {
      double re = crealf (syms[i]), im = cimagf (syms[i]);
      double dr = (re * cr - im * sr) / scale; /* de-rotated, unit power */
      double di = (re * sr + im * cr) / scale;
      double d  = (dr >= 0.0) ? 1.0 : -1.0; /* nearest BPSK point */
      errsq += (dr - d) * (dr - d) + di * di;
    }
  double evm = sqrt (errsq / (double)n); /* |ref| = 1 */
  return (evm > 0.0) ? 20.0 * log10 (evm) : -120.0;
}

/* Blind M2M4 Es/N0 (dB) over the back half. Returns -120.0 for a stream too
 * short to judge, so a symbol famine reads as an obvious sentinel rather than
 * a plausible number. */
static inline double
dp_test_m2m4_snr_db (const float complex *syms, size_t n_syms)
{
  if (n_syms < 20)
    return -120.0;
  size_t lo = n_syms / 2;
  return snr_m2m4_db (syms + lo, n_syms - lo);
}

#endif /* DP_SYM_TEST_H */
