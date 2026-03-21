#ifndef DP_POCKETFFT_H
#define DP_POCKETFFT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct pocketfft_plan pocketfft_plan;

  /* Create 1D plan */
  pocketfft_plan *pocketfft_plan_1d (size_t n, int sign);

  /* Create 2D plan */
  pocketfft_plan *pocketfft_plan_2d (size_t ny, size_t nx, int sign);

  /*
   * Execute 1D/2D.  in/out must point to an array of interleaved double
   * pairs (real, imag) — i.e. double complex[] in C, std::complex<double>[]
   * in C++.  Using void* keeps the header valid in both languages.
   */
  void pocketfft_execute_1d (pocketfft_plan *p, const void *in, void *out);

  void pocketfft_execute_2d (pocketfft_plan *p, const void *in, void *out);

  /* Destroy plan */
  void pocketfft_destroy_plan (pocketfft_plan *p);

#ifdef __cplusplus
}
#endif

#endif /* DP_POCKETFFT_H */
