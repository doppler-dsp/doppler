/**
 * @file resample_core.h
 * @brief Resample module — public C API.
 */
#ifndef RESAMPLE_CORE_H
#define RESAMPLE_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /* Declare module-level functions here. */

  double kaiser_beta (double atten);

  int kaiser_num_taps (int num_phases, double atten, double pb, double sb);

#ifdef __cplusplus
}
#endif

#endif /* RESAMPLE_CORE_H */
