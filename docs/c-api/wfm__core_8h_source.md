

# File wfm\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_core.h**](wfm__core_8h.md)

[Go to the documentation of this file](wfm__core_8h.md)


```C++

#ifndef WFM_CORE_H
#define WFM_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

void bpsk_map(const uint8_t *bits, size_t bits_len, float complex *out);

void qpsk_map(const uint8_t *syms, size_t syms_len, float complex *out);

float wfm_awgn_amplitude(float snr_db, float signal_power);

float wfm_ebno_to_snr_db(float ebno_db, int bits_per_symbol, float samples_per_symbol);

uint64_t mls_poly(uint32_t n);

uint16_t crc16(const uint8_t *bits, size_t bits_len);
void rrc_taps(double beta, int sps, int span, float *out);
void dsss_spread(const float complex *syms, size_t syms_len, const uint8_t *code, size_t code_len, int sf, float complex *out);
#ifdef __cplusplus
}
#endif

#endif /* WFM_CORE_H */
```


