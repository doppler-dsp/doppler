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
 *     noise, so a locked stream reads EVM[dB] ~ -(Es/N0)[dB]. (EVM is an
 *     I/Q-plane quantity: there is no factor of two — that belongs to an
 *     I-only measurement, and quoting it flatters the result by 3 dB.)
 *     **Pass the real `m`**; dp_test_evm_db_hard() is the BPSK spelling.
 *
 *     **A scattered stream does NOT read 0 dB except at BPSK.** Slicing a
 *     unit-modulus point at a uniformly random phase to its nearest of M
 *     neighbours leaves `E|e|^2 = 2 - 2 sin(pi/M)/(pi/M)`, so the floor is
 *     -1.4 dB at BPSK, -7.0 dB at QPSK and **-12.9 dB at 8PSK** — the last
 *     being what a perfectly healthy 13 dB link also reads. So an 8PSK EVM
 *     has under 3 dB of range between "on the bound at its SER=1e-3 anchor"
 *     and "no carrier recovery at all", and cannot carry a verdict by itself.
 *     dp_test_evm_scatter_floor_db() below computes the floor, and
 *     dp_ber_report() gates on it.
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

#include "ber/ber_core.h" /* the EVM / scatter-floor / settling primitives */
#include "snr/snr_core.h" /* snr_m2m4_db — the canonical blind estimator */
#include <complex.h>
#include <math.h>
#include <stddef.h>

/**
 * @brief Self-referenced EVM (dB) over an EXPLICIT window `[lo, hi)`.
 *
 * The range-taking form of dp_test_evm_db_hard_m(); see that function for what
 * the number means. Use this one whenever the EVM must be reported beside a
 * symbol error rate: **BER and EVM must be measured on the SAME window**, and
 * the convenience back-half spelling silently scores a different one (the back
 * half of whatever you handed it, which is the back QUARTER of a record if you
 * already sliced off the settling transient). An EVM and a BER taken over
 * different windows will eventually disagree, and the disagreement reads as a
 * receiver defect rather than as the harness bug it is.
 *
 * @param syms  Recovered symbols.
 * @param lo    First symbol scored.
 * @param hi    One past the last symbol scored.
 * @param m     Constellation order (2, 4, 8, ...); < 2 is treated as 2.
 * @return      EVM in dB, or 0.0 ("no lock") for a window under 20 symbols.
 */
static inline double
dp_test_evm_db_hard_range (const float complex *syms, size_t lo, size_t hi,
                           int m)
{
  return ber_evm_db (syms, hi, lo, hi, m);
}

/**
 * @brief Self-referenced EVM (dB) over the back half, for an M-PSK stream.
 *
 * Scores each symbol against the stream's OWN hard decision, with the
 * constellation rotation estimated from the data — so it references neither
 * the transmitted symbols nor a lag, and cannot be fooled by an alignment
 * search. At a matched-filter output the error vector IS the complex noise, so
 * a locked stream reads `EVM[dB] ~ -(Es/N0)[dB]`. (EVM is an I/Q-plane
 * quantity: no factor of two.) A SCATTERED stream reads
 * dp_test_evm_scatter_floor_db(m), which is 0 dB only in the BPSK limit — see
 * that function before writing any fixed threshold against this.
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
 * The back half is a convenience default, not a measurement window: prefer
 * dp_test_evm_db_hard_range() with the same `[lo, hi)` the error rate uses.
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
  return dp_test_evm_db_hard_range (syms, n_syms / 2, n_syms, m);
}

/* BPSK spelling, kept because most callers here are BPSK (the DSSS receivers)
 * and reads better at the call site than passing a literal 2. */
static inline double
dp_test_evm_db_hard (const float complex *syms, size_t n_syms)
{
  return dp_test_evm_db_hard_m (syms, n_syms, 2);
}

/**
 * @brief EVM (dB) of an M-PSK constellation at a UNIFORMLY RANDOM rotation.
 *
 * The FLOOR of dp_test_evm_db_hard_m(): what a completely destroyed
 * constant-modulus constellation reads. Slicing a unit-modulus point at a
 * uniformly random phase to its nearest of M neighbours leaves
 * `E|e|^2 = 2 - 2 sin(pi/M)/(pi/M)`, so
 *
 *     M = 2  ->   -1.4 dB
 *     M = 4  ->   -7.0 dB
 *     M = 8  ->  -12.9 dB
 *
 * **Any fixed EVM threshold MUST be stated against this, never against 0 dB.**
 * A `< -12.0` assertion is meaningless at 8PSK — a stream with no carrier
 * recovery at all passes it — and that is not hypothetical: it was live in
 * test_mpsk_receiver_r_core.c's every-M loop until 2026-07-27. The room
 * between "on the bound at the SER=1e-3 anchor" and "completely broken"
 * shrinks fast with M:
 *
 *     M = 2:  -6.8 dB anchor vs  -1.4 floor  ->  5.4 dB of range
 *     M = 4: -10.3 dB anchor vs  -7.0 floor  ->  3.3 dB
 *     M = 8: -15.7 dB anchor vs -12.9 floor  ->  2.8 dB
 *
 * So at 8PSK the self-referenced EVM cannot carry a verdict alone; pair it
 * with the truth-referenced error rate, which is what dp_ber_test.h does.
 *
 * @param m  Constellation order (2, 4, 8, ...); < 2 is treated as 2.
 * @return   The scatter floor in dB (negative).
 */
static inline double
dp_test_evm_scatter_floor_db (int m)
{
  return ber_evm_scatter_floor_db (m);
}

/* Blind M2M4 Es/N0 (dB) over an EXPLICIT window — the twin of
 * dp_test_evm_db_hard_range(), for the same reason: pair it with the window
 * the error rate used, not with a fraction of the record. Returns -120.0 for
 * a window too short to judge, so a symbol famine reads as an obvious
 * sentinel rather than a plausible number. */
static inline double
dp_test_m2m4_snr_db_range (const float complex *syms, size_t lo, size_t hi)
{
  if (hi <= lo || hi - lo < 20)
    return -120.0;
  return snr_m2m4_db (syms + lo, hi - lo);
}

/* Blind M2M4 Es/N0 (dB) over the back half. */
static inline double
dp_test_m2m4_snr_db (const float complex *syms, size_t n_syms)
{
  if (n_syms < 20)
    return -120.0;
  return dp_test_m2m4_snr_db_range (syms, n_syms / 2, n_syms);
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
  return ber_settle_syms (bn_timing, bn_carrier);
}

#endif /* DP_SYM_TEST_H */
