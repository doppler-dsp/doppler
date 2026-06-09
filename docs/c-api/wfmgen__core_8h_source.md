

# File wfmgen\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfmgen**](dir_2784f51dc2a964fe71c3814677da8805.md) **>** [**wfmgen\_core.h**](wfmgen__core_8h.md)

[Go to the documentation of this file](wfmgen__core_8h.md)


```C++

#ifndef WFMGEN_CORE_H
#define WFMGEN_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

void bpsk_map(const uint8_t *bits, size_t bits_len, float complex *out);

void qpsk_map(const uint8_t *syms, size_t syms_len, float complex *out);

float wfm_awgn_amplitude(float snr_db, float signal_power);

float wfm_ebno_to_snr_db(float ebno_db, int bits_per_symbol, float samples_per_symbol);
#ifdef __cplusplus
}
#endif

#endif /* WFMGEN_CORE_H */
```


