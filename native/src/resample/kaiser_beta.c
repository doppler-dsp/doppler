/*
 * kaiser_beta.c — resample module-level function.
 */
#include "doppler/resample/resample_core.h"

double
dp_kaiser_beta (double atten)
{
  /* The formula lives once, header-inline, so resamp's built-in bank uses
     the same code without linking this module (doppler#2652). */
  return dp_kaiser_beta_eval (atten);
}
