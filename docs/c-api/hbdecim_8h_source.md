

# File hbdecim.h

[**File List**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**hbdecim.h**](hbdecim_8h.md)

[Go to the documentation of this file](hbdecim_8h.md)


```C++


#ifndef DP_HBDECIM_H
#define DP_HBDECIM_H

#include <dp/stream.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct dp_hbdecim_cf32 dp_hbdecim_cf32_t;

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  dp_hbdecim_cf32_t *dp_hbdecim_cf32_create (size_t num_taps, const float *h);

  void dp_hbdecim_cf32_destroy (dp_hbdecim_cf32_t *r);

  void dp_hbdecim_cf32_reset (dp_hbdecim_cf32_t *r);

  /* ------------------------------------------------------------------
   * Properties
   * ------------------------------------------------------------------ */

  double dp_hbdecim_cf32_rate (const dp_hbdecim_cf32_t *r);

  size_t dp_hbdecim_cf32_num_taps (const dp_hbdecim_cf32_t *r);

  /* ------------------------------------------------------------------
   * Processing
   * ------------------------------------------------------------------ */

  size_t dp_hbdecim_cf32_execute (dp_hbdecim_cf32_t *r, const float _Complex *in,
                                  size_t num_in, float _Complex *out,
                                  size_t max_out);

  /* ==================================================================
   * Architecture D2 — real input, embedded fs/4 mix → complex IQ
   * ==================================================================
   *
   * @brief Real-input halfband decimator with embedded fs/4 frequency
   *        shift (Architecture D2).
   *
   * Combines a 2:1 halfband decimation with a zero-multiply fs/4
   * frequency shift by baking the per-tap rotation
   * e^{j(π/2)k} into the FIR coefficients at construction time.
   *
   * Input: real float32 samples at fs.
   * Output: complex CF32 at fs/2, containing the full [0, fs/4] band.
   *
   * Followed by a fine NCO at fs/2 and a DPMFS resampler, this forms
   * Architecture D2 — approximately 2× cheaper than Architecture D
   * for any real ADC input.
   *
   * ### Coefficient source
   *
   * Same as dp_hbdecim_cf32: pass the FIR branch of the polyphase
   * bank from `kaiser_prototype(phases=2)`.
   *
   * ### Usage
   *
   * ```c
   * #include <dp/hbdecim.h>
   *
   * // h[NUM_TAPS] = FIR branch from kaiser_prototype(phases=2)
   * dp_hbdecim_r2cf32_t *r =
   *     dp_hbdecim_r2cf32_create(NUM_TAPS, h);
   *
   * float  in[IN_LEN];
   * float _Complex out[IN_LEN / 2 + 2];
   * size_t n_out = dp_hbdecim_r2cf32_execute(
   *     r, in, IN_LEN, out,
   *     sizeof(out) / sizeof(out[0]));
   *
   * dp_hbdecim_r2cf32_destroy(r);
   * ```
   */

  typedef struct dp_hbdecim_r2cf32 dp_hbdecim_r2cf32_t;

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  dp_hbdecim_r2cf32_t *dp_hbdecim_r2cf32_create (size_t num_taps,
                                                 const float *h);

  void dp_hbdecim_r2cf32_destroy (dp_hbdecim_r2cf32_t *r);

  void dp_hbdecim_r2cf32_reset (dp_hbdecim_r2cf32_t *r);

  /* ------------------------------------------------------------------
   * Properties
   * ------------------------------------------------------------------ */

  double dp_hbdecim_r2cf32_rate (const dp_hbdecim_r2cf32_t *r);

  size_t dp_hbdecim_r2cf32_num_taps (const dp_hbdecim_r2cf32_t *r);

  /* ------------------------------------------------------------------
   * Processing
   * ------------------------------------------------------------------ */

  size_t dp_hbdecim_r2cf32_execute (dp_hbdecim_r2cf32_t *r, const float *in,
                                    size_t num_in, float _Complex *out,
                                    size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* DP_HBDECIM_H */
```
