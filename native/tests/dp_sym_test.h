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
 *   - dp_test_evm_db_hard_m() — EVM against the stream's OWN hard decisions,
 *     with the constellation rotation estimated from the data. No lag, no
 *     truth. At the matched-filter output the error vector IS the complex
 *     noise, so a locked stream reads EVM[dB] ~ -(Es/N0)[dB] and a scattered
 *     one sits near 0 dB. (EVM is an I/Q-plane quantity: there is no factor of
 *     two — that belongs to an I-only measurement, and quoting it flatters the
 *     result by 3 dB.) **Pass the real `m`**; dp_test_evm_db_hard() is the
 *     BPSK spelling of it.
 *   - dp_test_m2m4_snr_db() — the blind moment-based estimator, via the
 *     canonical snr_m2m4_db() primitive. Fully independent of the above: a
 *     locked stream recovers ~Es/N0, noise-dominated symbols estimate near 0.
 *
 * An EVM that BEATS the -(Es/N0) bound is the tell that the measurement is
 * wrong, not that the receiver is brilliant.
 *
 * Both score the BACK HALF of the stream, so an acquisition transient (or a
 * single cycle slip during it) cannot drag the steady-state figure — the
 * failure mode that once made a healthy eye read -17 dB. For where a settled
 * window may START, use dp_test_settle_syms() below rather than a fraction of
 * the record; a fraction is the other half of that same failure mode.
 */
#ifndef DP_SYM_TEST_H
#define DP_SYM_TEST_H

#include "snr/snr_core.h" /* snr_m2m4_db — the canonical blind estimator */
#include <complex.h>
#include <math.h>
#include <stddef.h>

/**
 * @brief Self-referenced EVM (dB) over the back half, for an M-PSK stream.
 *
 * Scores each symbol against the stream's OWN hard decision, with the
 * constellation rotation estimated from the data — so it references neither
 * the transmitted symbols nor a lag, and cannot be fooled by an alignment
 * search. At a matched-filter output the error vector IS the complex noise, so
 * a locked stream reads `EVM[dB] ~ -(Es/N0)[dB]` and a scattered one sits near
 * 0 dB. (EVM is an I/Q-plane quantity: no factor of two.)
 *
 * The rotation estimate is the M-fold generalisation of the familiar BPSK one:
 * `phi = arg(sum z^m) / m`, which at `m = 2` is exactly `0.5*atan2(Im z^2,
 * Re z^2)`. Decisions then slice to the nearest of the `m` unit-modulus
 * points.
 *
 * **Pass the real `m`.** Scoring an M-PSK stream against a BPSK slicer reads
 * ~0 dB no matter how clean the constellation is, because every symbol off the
 * real axis is charged as error — an EVM near 0 dB beside a *passing* symbol
 * error rate is that mistake, not a receiver fault. (This function was
 * BPSK-only until 2026-07-27, which is exactly how that reading was produced.)
 *
 * @param syms    Recovered symbols.
 * @param n_syms  How many; the back half is scored.
 * @param m       Constellation order (2, 4, 8, ...); < 2 is treated as 2.
 * @return        EVM in dB, or 0.0 ("no lock") if the stream is too short.
 */
static inline double
dp_test_evm_db_hard_m (const float complex *syms, size_t n_syms, int m)
{
  if (n_syms < 20)
    return 0.0;
  if (m < 2)
    m = 2;
  size_t lo = n_syms / 2, hi = n_syms, n = hi - lo;
  /* sum z^m (by repeated squaring/multiply — m is 2, 4 or 8 in practice) and
     the mean power, in one pass. */
  double smr = 0.0, smi = 0.0, p = 0.0;
  for (size_t i = lo; i < hi; i++)
    {
      double re = crealf (syms[i]), im = cimagf (syms[i]);
      p += re * re + im * im;
      double zr = re, zi = im; /* z^1 */
      for (int q = 1; q < m; q++)
        {
          double nr = zr * re - zi * im;
          zi        = zr * im + zi * re;
          zr        = nr;
        }
      smr += zr;
      smi += zi;
    }
  double scale = sqrt (p / (double)n);
  if (scale < 1e-20)
    return 0.0;
  double phi = atan2 (smi, smr) / (double)m; /* constellation rotation */
  double cr = cos (-phi), sr = sin (-phi);
  double step  = 2.0 * M_PI / (double)m;
  double errsq = 0.0;
  for (size_t i = lo; i < hi; i++)
    {
      double re = crealf (syms[i]), im = cimagf (syms[i]);
      double dr = (re * cr - im * sr) / scale; /* de-rotated, unit power */
      double di = (re * sr + im * cr) / scale;
      /* nearest of the m unit-modulus points */
      double th = step * (double)lround (atan2 (di, dr) / step);
      double er = dr - cos (th), ei = di - sin (th);
      errsq += er * er + ei * ei;
    }
  double evm = sqrt (errsq / (double)n); /* |ref| = 1 */
  return (evm > 0.0) ? 20.0 * log10 (evm) : -120.0;
}

/* BPSK spelling, kept because most callers here are BPSK (the DSSS receivers)
 * and reads better at the call site than passing a literal 2. */
static inline double
dp_test_evm_db_hard (const float complex *syms, size_t n_syms)
{
  return dp_test_evm_db_hard_m (syms, n_syms, 2);
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

/**
 * @brief Symbols to discard before a steady-state measurement means anything.
 *
 * A window pinned to a FRACTION of the record is the single most common way a
 * receiver test measures the acquisition transient and reports it as the
 * steady state. Derive it from the loops instead:
 *
 *   - **5/Bn per loop.** The standard settling time of a second-order loop at
 *     its noise bandwidth, in symbols, because both `bn` are normalised to the
 *     SYMBOL rate (so this number is invariant to samples-per-symbol).
 *   - **The two ADD**, because the loops are cascaded: the carrier
 *     discriminator reads the on-time strobe, so it cannot converge until
 *     timing has.
 *   - **Then double**, for joint tracking — each loop sees the other's
 *     transient as a disturbance while both are still moving.
 *
 * Measured cost of getting this wrong: reading from `5/bn` alone gave -9.0 dB
 * EVM where the settled answer is -23.2 dB, and a `size/4` window on a
 * 4000-symbol record reported SER 3.5e-2 on a decode that is EXACTLY zero from
 * the budget onward.
 *
 * Pass a loop's `bn` as 0 if it is not running. The Python twin is
 * `settle_floor()` in `src/doppler/track/tests/_mpsk_rx_harness.py`; keep them
 * in step.
 *
 * @param bn_timing   Timing loop noise bandwidth, per symbol (0 if none).
 * @param bn_carrier  Carrier loop noise bandwidth, per symbol (0 if none).
 * @return            Symbols to skip before measuring.
 */
static inline size_t
dp_test_settle_syms (double bn_timing, double bn_carrier)
{
  double s = 0.0;
  if (bn_timing > 0.0)
    s += 5.0 / bn_timing;
  if (bn_carrier > 0.0)
    s += 5.0 / bn_carrier;
  return (size_t)(2.0 * s);
}

#endif /* DP_SYM_TEST_H */
