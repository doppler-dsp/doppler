/**
 * @file awgn_core.h
 * @brief Additive White Gaussian Noise generator.
 *
 * Generates complex CF32 samples where real and imaginary parts are
 * independent zero-mean Gaussians, each with standard deviation
 * `amplitude`.  Total complex power = 2 * amplitude².
 *
 * ### Algorithm
 *
 * RNG: xoshiro256++ — four 64-bit state words, seeded via SplitMix64
 * from the user-supplied uint64 seed.  Period 2^256 − 1.
 *
 * Transform: Box-Muller.  Each call to awgn_generate() consumes two
 * 64-bit RNG outputs per complex output sample:
 *
 *   u1 ∈ (0, 1]  (top 24 bits of first 64-bit word, +1 offset, /2^24)
 *   u2 ∈ [0, 1)  (top 24 bits of second 64-bit word, /2^24)
 *   r     = amplitude * sqrt(−2 · ln u1)
 *   θ     = 2π · u2
 *   out   = r·cos θ  +  j·r·sin θ
 *
 * ### Usage
 *
 * @code
 * awgn_state_t *g = awgn_create(42, 1.0f);
 * float complex out[1024];
 * awgn_generate(g, 1024, out);
 * awgn_destroy(g);
 * @endcode
 */
#ifndef AWGN_CORE_H
#define AWGN_CORE_H

#include "clib_common.h"
#include "jm_perf.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    uint64_t s[4];      /* xoshiro256++ scalar state             */
    uint64_t seed;      /* initial seed stored for awgn_reset()  */
    float    amplitude;
    /* 8 independent xoshiro256++ streams for the AVX2 path.
     * vs[word][stream]: word ∈ {0,1,2,3}, stream ∈ {0..7}. */
    uint64_t vs[4][8];
  } awgn_state_t;

  /**
   * @brief Create an AWGN generator.
   *
   * @param seed       RNG seed.  Two generators with different seeds produce
   *                   uncorrelated noise streams.
   * @param amplitude  Per-component (Re, Im) standard deviation.  Must be ≥ 0.
   * @return Heap-allocated state, or NULL on allocation failure.
   */
  awgn_state_t *awgn_create (uint64_t seed, float amplitude);

  /** Free all resources.  NULL is a no-op. */
  void awgn_destroy (awgn_state_t *state);

  /** Reset RNG to the seed supplied at create time. */
  void awgn_reset (awgn_state_t *state);

  /** Return the current amplitude (per-component std dev). */
  float awgn_get_amplitude (const awgn_state_t *state);

  /** Set amplitude without disturbing RNG state. */
  void awgn_set_amplitude (awgn_state_t *state, float val);

  /** Reseed the RNG and reset state. */
  void awgn_reseed (awgn_state_t *state, uint64_t seed);

  /**
   * @brief Conservative upper bound on generate() output size.
   *
   * Returns 65536.  The Python extension uses this for the initial
   * buffer allocation; the buffer grows on demand if n > 65536.
   */
  size_t awgn_generate_max_out (awgn_state_t *state);

  /**
   * @brief Generate n complex CF32 AWGN samples.
   *
   * @param state  Must be non-NULL.
   * @param n      Number of samples to generate.
   * @param out    Output buffer, capacity ≥ n.
   * @return n (always; generate produces exactly n samples).
   */
  size_t awgn_generate (awgn_state_t *state, size_t n, float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* AWGN_CORE_H */
