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
 * Transform: Box-Muller.  Each call to awgn_generate () consumes two
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
 * awgn_generate(g, 1024, out, 1024);
 * awgn_destroy(g);
 * @endcode
 */
#ifndef AWGN_CORE_H
#define AWGN_CORE_H

#include "clib_common.h"
#include "dp_state.h"
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
   * Allocates state, seeds the xoshiro256++ RNG via SplitMix64, and
   * sets up both the scalar and the AVX2 parallel streams.  The initial
   * seed is stored so awgn_reset() can reproduce the exact same stream.
   *
   * @param seed       64-bit RNG seed.  Two generators with different seeds
   *                   produce statistically independent noise streams.
   * @param amplitude  Per-component (Re, Im) standard deviation.  Must be
   *                   ≥ 0; total complex power = 2 × amplitude².
   * @return Heap-allocated state, or NULL on allocation failure.
   * @code
   * >>> from doppler.source import AWGN
   * >>> gen = AWGN(seed=0, amplitude=1.0)
   * >>> gen.amplitude
   * 1.0
   * @endcode
   */
  awgn_state_t *awgn_create (uint64_t seed, float amplitude);

  /** Free all resources.  NULL is a no-op. */
  void awgn_destroy (awgn_state_t *state);

  /**
   * @brief Reset RNG to the seed supplied at create time.
   * Re-runs the SplitMix64 seeding procedure with the original seed so
   * the next awgn_generate() call produces exactly the same samples as
   * the first call after awgn_create().  amplitude is not changed.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.source import AWGN
   * >>> gen = AWGN(seed=0, amplitude=1.0)
   * >>> first = gen.generate(4)
   * >>> gen.reset()
   * >>> second = gen.generate(4)
   * >>> bool(np.all(first == second))
   * True
   * @endcode
   */
  void awgn_reset (awgn_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ────────
   * Serializes the running RNG state — the scalar xoshiro256++ state s[4] and
   * the 8 AVX2 stream states vs[4][8] — so a resumed generator continues the
   * exact same noise sequence.  seed / amplitude are config (constructor).
   * Envelope: [dp_state_hdr_t][u64 s[4]][u64 vs[4][8]]. */
#define AWGN_STATE_MAGIC DP_FOURCC ('A', 'W', 'G', 'N')
#define AWGN_STATE_VERSION 1u

  /** @brief Serialized-state byte size. */
  size_t awgn_state_bytes (const awgn_state_t *state);
  /** @brief Serialize the RNG state (scalar + AVX2 streams) into @p blob. */
  void awgn_get_state (const awgn_state_t *state, void *blob);
  /** @brief Restore RNG state; DP_OK, or DP_ERR_INVALID if rejected. */
  int awgn_set_state (awgn_state_t *state, const void *blob);

  /**
   * @brief Return the current amplitude (per-component std dev).
   * @code
   * >>> from doppler.source import AWGN
   * >>> gen = AWGN(seed=0, amplitude=1.0)
   * >>> gen.amplitude
   * 1.0
   * >>> gen.amplitude = 2.0
   * >>> gen.amplitude
   * 2.0
   * @endcode
   */
  float awgn_get_amplitude (const awgn_state_t *state);

  /**
   * @brief The @p amplitude that puts a signal at a target SNR.
   *
   * The inverse of this generator's own convention, and the reason it lives
   * here: @ref awgn_create takes a PER-COMPONENT sigma, so the complex noise
   * power it produces is `2 * amplitude^2`. For a signal of power
   * @p signal_power at @p snr_db (referenced to the full sample rate),
   *
   *   `amplitude = sqrt(signal_power / (2 * 10^(snr_db/10)))`
   *
   * Pass the result straight to awgn_create(). "Is the amplitude per rail or
   * total power?" has two defensible answers and this function is the one
   * place that answers it -- a caller deriving its own sigma is one factor of
   * two away from a 3 dB error that nothing will fail on.
   *
   * @param snr_db        Target SNR in dB, over the full sample rate.
   * @param signal_power  Signal power (1.0 for unit-power tones or
   *                      unit-energy BPSK/QPSK symbols).
   * @return Per-component sigma for one I or Q rail.
   */
  float awgn_amplitude_for_snr (float snr_db, float signal_power);

  /** Set amplitude without disturbing RNG state. */
  void awgn_set_amplitude (awgn_state_t *state, float val);

  /**
   * @brief Reseed the RNG and reset all xoshiro256++ state.
   * Equivalent to calling awgn_destroy() and awgn_create(seed, amplitude)
   * but reuses the existing allocation.  amplitude is unchanged.
   *
   * @param state  Generator state returned by awgn_create().
   * @param seed   New 64-bit RNG seed.
   * @code
   * >>> import numpy as np
   * >>> from doppler.source import AWGN
   * >>> gen = AWGN(seed=0, amplitude=1.0)
   * >>> gen.reseed(42)
   * >>> out1 = gen.generate(4)
   * >>> gen2 = AWGN(seed=42, amplitude=1.0)
   * >>> out2 = gen2.generate(4)
   * >>> bool(np.all(out1 == out2))
   * True
   * @endcode
   */
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
   * Uses Box-Muller with xoshiro256++ to fill `out` with independent
   * complex Gaussians: Re and Im each have zero mean and standard
   * deviation `amplitude`.  Total complex power = 2 × amplitude².
   * The AVX2 path processes 8 samples in parallel when available.
   *
   * @param state  Generator state returned by awgn_create().
   * @param n      Number of samples to generate.
   * @param out    Output buffer; must hold at least n float complex values.
   * @param max_out Capacity of @p out in elements. Emission stops there, so
   *                the return value is the number actually written.
   * @return min(n, max_out) samples.
   * @code
   * >>> import numpy as np
   * >>> from doppler.source import AWGN
   * >>> gen = AWGN(seed=0, amplitude=1.0)
   * >>> out = gen.generate(1024)
   * >>> out.dtype
   * dtype('complex64')
   * >>> out.shape
   * (1024,)
   * >>> round(float(np.var(out.real)), 1)
   * 1.0
   * >>> round(float(np.var(out.imag)), 1)
   * 1.0
   * @endcode
   */
  size_t awgn_generate (awgn_state_t *state, size_t n, float complex *out,
                        size_t max_out);

  /**
   * @brief One-shot AWGN generation — no persistent state required.
   *
   * Creates a temporary generator, fills `out`, then frees it.
   * Equivalent to:
   * @code
   * awgn_state_t *g = awgn_create(seed, amplitude);
   * awgn_generate (g, n, out, n);
   * awgn_destroy(g);
   * @endcode
   *
   * @param seed       RNG seed.
   * @param amplitude  Per-component (Re, Im) standard deviation.
   * @param n          Number of samples to generate.
   * @param out        Output buffer, capacity ≥ n.
   * @return DP_OK on success, DP_ERR_MEMORY on allocation failure.
   */
  int awgn (uint64_t seed, float amplitude, size_t n, float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* AWGN_CORE_H */
