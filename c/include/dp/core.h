/**
 * @file core.h
 * @brief Core DSP engine — initialisation and version.
 *
 * Call dp_init() once before using any DSP function.
 * Call dp_cleanup() at shutdown to release FFTW plans and global state.
 *
 * ```c
 * #include <dp/core.h>
 * dp_init();
 * // ... use FFT, SIMD, etc. ...
 * dp_cleanup();
 * ```
 */

#ifndef DP_CORE_H
#define DP_CORE_H

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Initialise the Doppler DSP engine.
   *
   * Must be called once before any other DSP function.  Initialises FFTW
   * thread support and performs SIMD feature detection.
   *
   * @return 0 on success, non-zero on failure.
   */
  int dp_init (void);

  /**
   * @brief Clean up global Doppler state.
   *
   * Releases FFTW plans and any other global resources.  Safe to call even
   * if dp_init() was never called.
   */
  void dp_cleanup (void);

  /**
   * @brief Return the library version string (e.g. `"1.0.0"`).
   * @return Statically allocated, null-terminated string.
   */
  const char *dp_version (void);

  /**
   * @brief Return a build-info string describing compile-time features.
   *
   * Includes SIMD level, FFTW backend, and compiler details.
   *
   * @return Statically allocated, null-terminated string.
   */
  const char *dp_build_info (void);

#ifdef __cplusplus
}
#endif

#endif /* DP_CORE_H */
