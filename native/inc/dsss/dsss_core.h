/**
 * @file dsss_core.h
 * @brief Dsss module — public C API.
 */
#ifndef DSSS_CORE_H
#define DSSS_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Declare module-level functions here. */

/**
 * @brief Map an FFT bin index to its SIGNED frequency index.
 *
 * @param bin     Bin index in `[0, n_bins)`.
 * @param n_bins  Grid size.
 * @return Signed index in `[-(n_bins/2), +((n_bins-1)/2)]`.
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import bin_to_signed
 * >>> [bin_to_signed(b, 8) for b in range(8)]
 * [0, 1, 2, 3, -4, -3, -2, -1]
 * >>> (np.fft.fftfreq(8) * 8).astype(int).tolist()   # same convention
 * [0, 1, 2, 3, -4, -3, -2, -1]
 * >>> bin_to_signed(4, 7)                         # odd grid: no ambiguity
 * -3
 *
 * @endcode
 */
int bin_to_signed(size_t bin, size_t n_bins);
#ifdef __cplusplus
}
#endif

#endif /* DSSS_CORE_H */
