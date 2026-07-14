/**
 * @file filter_core.h
 * @brief Filter module — public C API.
 */
#ifndef FILTER_CORE_H
#define FILTER_CORE_H

#include "clib_common.h"
#include "resample/resample_core.h" /* kaiser_num_taps — used by design_lowpass's
                                        generated out_size allocation expression */

#ifdef __cplusplus
extern "C"
{
#endif

  /* Declare module-level functions here. */

void design_lowpass(double fpass, double fstop, double atten_db, float *out);
#ifdef __cplusplus
}
#endif

#endif /* FILTER_CORE_H */
