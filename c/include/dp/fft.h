/**
 * @file fft.h
 * @brief Fast Fourier Transform — setup and execution.
 *
 * Backed by PocketFFT (header-only, bundled) or FFTW3 when available.
 * A single global plan is maintained; call dp_fft_global_setup()
 * once per shape before executing transforms.
 *
 * ### 1-D example
 * ```c
 * #include <dp/fft.h>
 * #include <complex.h>
 *
 * size_t shape[] = { 1024 };
 * dp_fft_global_setup(shape, 1, -1, 1, "estimate", NULL);
 *
 * double complex in[1024], out[1024];
 * // ... fill in[] ...
 * dp_fft1d_execute(in, out);
 * ```
 */

#ifndef DP_FFT_H
#define DP_FFT_H

#include <complex.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Set up the global FFT plan for a given shape.
   *
   * Plans are cached by shape and parameters; subsequent calls with the same
   * shape return immediately. When FFTW is the backend, @p planner and
   * @p wisdom_path control FFTW plan creation.
   *
   * @param shape        Array of @p ndim dimension sizes (e.g. `{1024}` for
   * 1-D).
   * @param ndim         Number of dimensions: 1 or 2.
   * @param sign         Transform direction: -1 for forward FFT, +1 for
   * inverse.
   * @param nthreads     Number of threads (FFTW only; ignored by PocketFFT).
   * @param planner      Planner effort: `"estimate"`, `"measure"`,
   * `"patient"`, or `"exhaustive"` (FFTW only).
   * @param wisdom_path  Path to load/save FFTW wisdom, or NULL to skip.
   *
   * @note **IMPORTANT (FFTW only)**: Heavier planners (`"measure"`,
   * `"patient"`,
   *       `"exhaustive"`) will **ERASE** the contents of the input and output
   *       arrays during the @p execution phase of the *first* transform (when
   * the plan is created). If using these planners, ensure you run the first
   *       transform with dummy data or call setup *before* filling your
   * buffers. The `"estimate"` planner (default) is safe to use on live data.
   */
  void dp_fft_global_setup (const size_t *shape, size_t ndim, int sign,
                            int nthreads, const char *planner,
                            const char *wisdom_path);

  /**
   * @brief Execute an out-of-place 1-D FFT.
   *
   * Uses the plan established by the most recent dp_fft_global_setup()
   * call with ndim=1.  @p input and @p output must each be at least
   * `shape[0]` elements.
   *
   * @param input  Read-only input array of complex doubles.
   * @param output Output array (must not alias @p input).
   */
  void dp_fft1d_execute (const double complex *input, double complex *output);

  /**
   * @brief Execute an in-place 1-D FFT.
   *
   * @param data  Input/output array of `shape[0]` complex doubles.
   */
  void dp_fft1d_execute_inplace (double complex *data);

  /**
   * @brief Execute an out-of-place 2-D FFT.
   *
   * Uses the plan established by the most recent dp_fft_global_setup()
   * call with ndim=2.  Arrays must each hold `shape[0] * shape[1]` elements
   * in row-major order.
   *
   * @param input  Read-only input array.
   * @param output Output array (must not alias @p input).
   */
  void dp_fft2d_execute (const double complex *input, double complex *output);

  /**
   * @brief Execute an in-place 2-D FFT.
   *
   * @param data  Input/output array of `shape[0] * shape[1]` complex doubles.
   */
  void dp_fft2d_execute_inplace (double complex *data);

#ifdef __cplusplus
}
#endif

#endif /* DP_FFT_H */
