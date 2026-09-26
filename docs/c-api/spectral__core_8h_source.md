

# File spectral\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**spectral**](dir_bb80cb4693a043f62f72e53e1f90a405.md) **>** [**spectral\_core.h**](spectral__core_8h.md)

[Go to the documentation of this file](spectral__core_8h.md)


```C++

#ifndef DP_SPECTRAL_CORE_H
#define DP_SPECTRAL_CORE_H

#include "doppler/clib_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    float freq_norm;    
    float amplitude_db; 
  } dp_peak_t;

float dp_kaiser_enbw(const float *w, size_t w_len);

  double dp_kaiser_beta_for_sidelobe(double atten_db);

void dp_kaiser_window(float *w, size_t w_len, float beta);

void dp_hann_window(float *w, size_t w_len);

void dp_blackman_harris_window(float *w, size_t w_len);

void dp_magnitude_db_cf32(const float _Complex *x, size_t x_len, float *out, float lin_floor, float offset_db);

void dp_magnitude_db_cf64(const double _Complex *x, size_t x_len, float *out, double lin_floor, float offset_db);

size_t dp_find_peaks_f32(const float *db, size_t db_len, size_t n_peaks, float min_db, dp_peak_t *result);

double dp_obw_from_power(const double *pwr, size_t pwr_len, double fs, double frac);
double dp_noise_floor_db(const float *db, size_t db_len);
#ifdef __cplusplus
}
#endif

#endif /* SPECTRAL_CORE_H */
```


