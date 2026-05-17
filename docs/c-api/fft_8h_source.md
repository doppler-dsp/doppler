

# File fft.h

[**File List**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**fft.h**](fft_8h.md)

[Go to the documentation of this file](fft_8h.md)


```C++


#ifndef DP_FFT_H
#define DP_FFT_H

#include <complex.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  void dp_fft_global_setup (const size_t *shape, size_t ndim, int sign,
                            int nthreads, const char *planner,
                            const char *wisdom_path);

  void dp_fft1d_execute (const double complex *input, double complex *output);

  void dp_fft1d_execute_inplace (double complex *data);

  void dp_fft2d_execute (const double complex *input, double complex *output);

  void dp_fft2d_execute_inplace (double complex *data);

  /* ------------------------------------------------------------------
   * Single-precision (float complex / CF32) variants
   * ------------------------------------------------------------------
   * These share the same global shape and direction set by
   * dp_fft_global_setup(), but maintain a separate internal plan.
   * Use them when 32-bit precision is sufficient (e.g. SDR signal
   * processing) and memory bandwidth or throughput is a concern.
   * ------------------------------------------------------------------ */

  void dp_fft1d_execute_cf32 (const float complex *input,
                               float complex *output);

  void dp_fft1d_execute_inplace_cf32 (float complex *data);

  void dp_fft2d_execute_cf32 (const float complex *input,
                               float complex *output);

  void dp_fft2d_execute_inplace_cf32 (float complex *data);

  /* ------------------------------------------------------------------
   * Per-instance plan API (thread-safe, no global state)
   * ------------------------------------------------------------------
   * Each dp_fft_t / dp_fft2d_t is an independent, heap-allocated plan.
   * Multiple instances of different sizes, signs, or precisions may
   * coexist and be used concurrently from different threads.
   *
   * ### Example
   * ```c
   * dp_fft_t *p = dp_fft_create(1024, -1, 1);
   * float complex in[1024], out[1024];
   * // ... fill in[] ...
   * dp_fft_execute_cf32(p, in, out);
   * dp_fft_destroy(p);
   * ```
   * ------------------------------------------------------------------ */

  typedef struct dp_fft_s dp_fft_t;

  typedef struct dp_fft2d_s dp_fft2d_t;

  dp_fft_t *dp_fft_create (size_t n, int sign, int nthreads);

  void dp_fft_destroy (dp_fft_t *plan);

  dp_fft2d_t *dp_fft2d_create (size_t ny, size_t nx, int sign, int nthreads);

  void dp_fft2d_destroy (dp_fft2d_t *plan);

  /* 1-D execute (per-instance; distinct from global dp_fft1d_execute*) */
  void dp_fft_run_cf64 (const dp_fft_t *plan,
                        const double complex *in, double complex *out);
  void dp_fft_run_cf32 (const dp_fft_t *plan,
                        const float complex *in, float complex *out);
  void dp_fft_run_inplace_cf64 (const dp_fft_t *plan, double complex *data);
  void dp_fft_run_inplace_cf32 (const dp_fft_t *plan, float complex *data);

  /* 2-D execute (per-instance; distinct from global dp_fft2d_execute*) */
  void dp_fft2d_run_cf64 (const dp_fft2d_t *plan,
                          const double complex *in, double complex *out);
  void dp_fft2d_run_cf32 (const dp_fft2d_t *plan,
                          const float complex *in, float complex *out);
  void dp_fft2d_run_inplace_cf64 (const dp_fft2d_t *plan, double complex *data);
  void dp_fft2d_run_inplace_cf32 (const dp_fft2d_t *plan, float complex *data);

#ifdef __cplusplus
}
#endif

#endif /* DP_FFT_H */
```
