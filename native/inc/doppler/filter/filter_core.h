/**
 * @file filter_core.h
 * @brief Filter module — public C API.
 */
#ifndef DP_FILTER_CORE_H
#define DP_FILTER_CORE_H

#include "doppler/clib_common.h"
#include "doppler/resample/resample_core.h" /* kaiser_num_taps — used by design_lowpass's
                                        generated out_size allocation expression */

#ifdef __cplusplus
extern "C"
{
#endif

  /* Declare module-level functions here. */

void dp_design_lowpass(double fpass, double fstop, double atten_db, float *out);
#ifdef __cplusplus
}
#endif

#endif /* FILTER_CORE_H */
