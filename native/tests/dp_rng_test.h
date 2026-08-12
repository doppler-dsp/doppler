/**
 * @file dp_rng_test.h
 * @brief The SSOT for harness RANDOMNESS: one generator, one Box-Muller.
 *
 * The third member of the `dp_*_test.h` family to be carved out of the
 * duplication rather than designed for it. `dp_test.h` took the assertions,
 * `dp_tx_test.h` took the shaped symbol stream; this takes the pseudo-random
 * numbers underneath both — the bit stream a test modulates, and the noise it
 * adds.
 *
 * ## What was actually there
 *
 * Every 32-bit copy in `native/tests/` already agreed on the generator:
 * Marsaglia's xorshift32 with the (13, 17, 5) triple, in that order. What
 * they disagreed about was everything around it, and the disagreements were
 * invisible because they all produced plausible-looking noise:
 *
 * | helper            | copies | distinct implementations              |
 * | ----------------- | ------ | ------------------------------------- |
 * | the xorshift step | 20     | 1 (inlined 9x, wrapped 11x)           |
 * | `prbs` -> +-1     | 6      | 1                                     |
 * | `prbs` -> 16 bits | 2      | 1 — **same name, different contract** |
 * | `uni` -> (0, 1]   | 2      | 1                                     |
 * | `gauss`           | 5      | 2                                     |
 * | `cgauss`          | 5      | 1                                     |
 *
 * Four of the five `gauss` copies were the same arithmetic written out four
 * times. The fifth, in `test_costas_core.c`, was a **half-finished edit** left
 * in the tree:
 *
 * @code
 * double u1 = (prbs (st) + 2) / 4.0;   // crude but seeded; reseed below
 * (void)u1;
 * uint32_t a = (*st ^= *st << 7, *st);
 * uint32_t b = (*st ^= *st >> 9, *st);
 * @endcode
 *
 * It burns one xorshift word into a variable it then voids, and draws its two
 * uniforms from a two-shift recurrence that is not xorshift32 and is not
 * full-period — with `b` one shift-xor away from `a`, so the two arguments to
 * Box-Muller are not independent. Measured over 2e6 draws against the N(0,1)
 * it claimed to be:
 *
 * | statistic | ideal   | that copy | this header |
 * | --------- | ------- | --------- | ----------- |
 * | mean      | 0       | **+0.056**| +0.0002     |
 * | variance  | 1       | **1.115** | 1.0015      |
 * | kurtosis  | 3       | **3.62**  | 2.997       |
 * | P(\|x\|>2s) | 0.0455| **0.0630**| 0.0455      |
 *
 * So the file's one AWGN test was running at 0.47 dB more noise than the
 * `sigma` it stated, on a distribution with a DC bias and a 38% heavy tail —
 * a Costas loop's phase detector sees a mean offset as signal. Nothing failed,
 * because the assertion had margin. That is the shape of this whole class of
 * bug: a private generator cannot be wrong in a way anything notices.
 *
 * ## Consequences of adopting this header
 *
 * For twenty of the twenty-one files it is bit-exact — the shifts, their
 * order, the `(x + 1) / 4294967297.0` uniform mapping and the draw order are
 * all preserved, so every stream is the one that file already had. Only
 * `test_costas_core.c` changes, and it changes because the thing it had was
 * broken. See that file and the PR that introduced this header.
 *
 * ## The design decisions worth stating
 *
 * **`dp_gauss` returns `double`, not `float`.** Four of the five copies
 * returned `float` and one returned `double` off identical arithmetic. Widest
 * wins: narrowing is a cast the call site can make and the header cannot
 * un-make. Call sites that were bit-exact on `float` keep an explicit
 * `(float)` so the rounding they had is still visible in their own code.
 *
 * **`dp_cgauss` is not `dp_gauss() + I * dp_gauss()`.** It consumes TWO words
 * and uses BOTH Box-Muller outputs — the `cos` branch for the real part and
 * the `sin` branch for the imaginary — scaled by `sqrt(-log u1)` rather than
 * `sqrt(-2 log u1)`, giving 0.5 variance per component and `E|z|^2 = 1`. The
 * sum form would consume four words, throw half of them away, and land at
 * unit variance PER COMPONENT, i.e. `E|z|^2 = 2`. Both are defensible
 * conventions for "complex Gaussian" and that is exactly why the one the
 * suite actually uses is spelled out here instead of re-derived per file.
 *
 * **The 32- and 64-bit generators are separate, and both stay.** The 32-bit
 * uniform has 32 bits of resolution; `test_dp_ber.c` measures error rates
 * down into the tail of the curve and draws its uniforms from a xorshift64
 * with a 53-bit mantissa, which is a reason, not an accident. Folding it onto
 * `dp_xs32` would coarsen the one test that needs the resolution.
 *
 * **A zero seed is absorbing, so it is not permitted to be silent.** State 0
 * is a fixed point of every xorshift; a test seeded from a counter that
 * happens to start at 0 would draw an all-zeros "random" stream forever and
 * pass. Several files wrote `seed ? seed : 1` by hand and the rest did not.
 * `dp_xs32` / `dp_xs64` substitute 1 for 0 themselves. This cannot change an
 * existing stream: xorshift is a bijection on the nonzero states, so a
 * generator that starts nonzero never reaches 0.
 *
 * **The cost of that substitution: seeds 0 and 1 alias.** They are the same
 * stream, so a sweep written `for (uint32_t s = 0; s < N; s++)` gets `N-1`
 * distinct realizations while reporting `N` — a small bias in exactly the
 * Monte-Carlo-over-seeds use this header is for. Mixing the seed instead
 * (`x = *st ^ 0x9e3779b9u`) would close it and is deliberately NOT done:
 * every existing stream would move, and the bit-exactness of the whole
 * migration onto this header is the property that made it reviewable. Start
 * seed sweeps at 1.
 *
 * ## Usage
 *
 * @code
 * #include "dp_rng_test.h"
 * #include "dp_test.h"
 *
 * uint32_t bst = 12345u;              // data stream
 * uint32_t nst = 12345u ^ 0x9e3779b9u;// noise stream, independently seeded
 * for (size_t i = 0; i < n; i++)
 *   x[i] = (float)dp_bit (&bst) + (float)(sigma * dp_gauss (&nst));
 * @endcode
 *
 * Keep separate streams in separate state variables when a test needs the
 * data and the noise to be independently reproducible — the convention the
 * suite already uses is `nst = seed ^ 0x9e3779b9u` for the noise arm.
 *
 * ## Never draw twice from one state inside one expression
 *
 * @code
 * z = (float)dp_gauss (&st) + (float)dp_gauss (&st) * I;  // WRONG
 * @endcode
 *
 * Two calls in one expression are **indeterminately sequenced** (C11
 * 6.5.2.2p10): not undefined behaviour, but the order is the compiler's, and
 * **gcc and clang genuinely disagree** — gcc evaluates the imaginary operand
 * first, clang the real one, at both `-O0` and `-O2`. doppler builds with
 * both (`make test` is gcc, `make coverage` is clang), so such a line draws
 * two different noise streams depending on the job.
 *
 * This was not hypothetical: eleven sites across seven files had it, seven of
 * them genuinely divergent. Draw into named locals instead —
 *
 * @code
 * double n_im = dp_gauss (&st);
 * double n_re = dp_gauss (&st);
 * z = (float)n_re + (float)n_im * I;
 * @endcode
 *
 * — and `make lint` rejects the one-expression form, so this paragraph is not
 * what is holding the line.
 *
 * ## Where a new generator goes
 *
 * Nowhere, ideally. If a test needs a distribution this header does not have,
 * add it HERE and say what convention it fixes — that is the whole lesson of
 * the table at the top. A shape genuinely used by one test (`test_psd_core.c`
 * draws uniform [-1, 1] from an LCG to fill a spectral estimate) stays static
 * in that test; a second caller is what makes it shared.
 */
#ifndef DP_RNG_TEST_H
#define DP_RNG_TEST_H

#include <complex.h>
#include <math.h>
#include <stdint.h>

/** 2*pi, spelled once. */
#define DP_RNG_TWO_PI 6.283185307179586

/**
 * Advance a xorshift32 state and return it.
 *
 * Marsaglia's (13, 17, 5) triple, left-right-left. A zero state is replaced
 * by 1 on entry: 0 is a fixed point, so a zero seed would otherwise yield an
 * endless run of zeros that reads as a passing test.
 *
 * @param st  the caller's state; advanced in place.
 * @return    the new state, uniform over the 2^32-1 nonzero words.
 */
static inline uint32_t
dp_xs32 (uint32_t *st)
{
  uint32_t x = *st ? *st : 1u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *st = x;
  return x;
}

/**
 * A uniform double in (0, 1] — open at zero, which is what matters.
 *
 * `log(u)` is the first thing Box-Muller does with it, so the interval must
 * exclude 0. The `(x + 1) / (2^32 + 1)` mapping is how every copy in the
 * suite achieved that, and it is preserved exactly so their streams are.
 *
 * @param st  the caller's state; advanced by one word.
 */
static inline double
dp_uni (uint32_t *st)
{
  return ((double)dp_xs32 (st) + 1.0) / 4294967297.0;
}

/**
 * A +-1 BPSK-style bit, from the low bit of one word.
 *
 * @param st  the caller's state; advanced by one word.
 * @return    -1 when the low bit is set, +1 otherwise.
 */
static inline int
dp_bit (uint32_t *st)
{
  return (dp_xs32 (st) & 1u) ? -1 : 1;
}

/**
 * A standard normal, N(0, 1), by Box-Muller.
 *
 * Consumes two words and keeps the `cos` branch; the `sin` branch is
 * discarded rather than cached, so the number of words drawn per call is a
 * constant and a stream stays reproducible under any call pattern. See
 * dp_cgauss() when both branches are wanted.
 *
 * @param st  the caller's state; advanced by two words.
 */
static inline double
dp_gauss (uint32_t *st)
{
  double u1 = dp_uni (st);
  double u2 = dp_uni (st);
  return sqrt (-2.0 * log (u1)) * cos (DP_RNG_TWO_PI * u2);
}

/**
 * A circularly-symmetric complex Gaussian with `E|z|^2 = 1`.
 *
 * Both Box-Muller branches from two words, scaled by `sqrt(-log u1)` =
 * `sqrt(-2 log u1) / sqrt(2)`, so each component carries variance 0.5 and the
 * total power is unity. This is the AWGN convention the receiver tests are
 * written against: a caller scaling by `sigma` gets noise power `sigma^2`.
 *
 * @param st  the caller's state; advanced by two words.
 */
static inline float complex
dp_cgauss (uint32_t *st)
{
  double u1  = dp_uni (st);
  double u2  = dp_uni (st);
  double mag = sqrt (-log (u1));
  double th  = DP_RNG_TWO_PI * u2;
  return (float)(mag * cos (th)) + (float)(mag * sin (th)) * I;
}

/* ── 64-bit ──────────────────────────────────────────────────────────────
 *
 * A second width, not a second opinion. `test_dp_ber.c` measures error rates
 * into the tail of the theoretical curve, where a 32-bit uniform's resolution
 * is the measurement floor. These draw a full double mantissa. */

/**
 * Advance a xorshift64 state and return it. Marsaglia's (13, 7, 17) triple;
 * a zero state is replaced by 1, as in dp_xs32().
 *
 * @param st  the caller's state; advanced in place.
 */
static inline uint64_t
dp_xs64 (uint64_t *st)
{
  uint64_t x = *st ? *st : 1u;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  *st = x;
  return x;
}

/**
 * A uniform double in (0, 1] with all 53 mantissa bits populated.
 *
 * @param st  the caller's state; advanced by one word.
 */
static inline double
dp_uni64 (uint64_t *st)
{
  return ((double)(dp_xs64 (st) >> 11) + 1.0) / 9007199254740993.0;
}

/**
 * A standard normal from the 64-bit generator. Box-Muller, `cos` branch.
 *
 * The two draws are sequenced into named variables rather than left as two
 * calls inside one expression, which is how `test_dp_ber.c` had it:
 *
 * @code
 * return sqrt (-2.0 * log (uni (s))) * cos (2.0 * MPSK_PI * uni (s));
 * @endcode
 *
 * The two calls are **indeterminately sequenced** (C11 6.5.2.2p10) — not
 * unsequenced, so there is no undefined behaviour; one of the two orders
 * happens, and which one is the compiler's choice. The orders give different
 * noise: the same seed produces 6.3417 one way and 2.3548 the other. gcc at
 * -O2 on x86-64 picks left-to-right, so writing that order out preserves the
 * stream this suite has been measuring against and makes it stop depending on
 * the compiler.
 *
 * @param st  the caller's state; advanced by two words.
 */
static inline double
dp_gauss64 (uint64_t *st)
{
  double u1 = dp_uni64 (st);
  double u2 = dp_uni64 (st);
  return sqrt (-2.0 * log (u1)) * cos (DP_RNG_TWO_PI * u2);
}

#endif /* DP_RNG_TEST_H */
